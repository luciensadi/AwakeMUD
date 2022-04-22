// File describes the spec and helpers for the initiative tracker, which is
// deployed in all the Nerpcorpolis rooms. - LS

#include "awake.h"
#include "structs.h"
#include "comm.h"
#include "interpreter.h"
#include "db.h"
#include "handler.h"

char tracker_primary_buf[2000];

struct obj_data *_find_name_in_tracker(struct obj_data *tracker, const char *name, bool must_be_exact) {
  // Look for exact match.
  for (struct obj_data *entry = tracker->contains; entry; entry = entry->next_content) {
    if (!str_cmp(name, GET_OBJ_NAME(entry)))
      return entry;
  }

  // Look for partial match.
  if (!must_be_exact) {
    for (struct obj_data *entry = tracker->contains; entry; entry = entry->next_content) {
      if (is_abbrev(name, GET_OBJ_NAME(entry)))
        return entry;
    }
  }

  return NULL;
}

#define GET_TRACKER_ENTRY_INIT_VALUE(entry)   (GET_OBJ_VAL((entry), 0))
#define GET_TRACKER_ENTRY_PLR_IDNUM(entry)    (GET_OBJ_VAL((entry), 1))

#define GET_TRACKER_LOCK_IDNUM(tracker) (GET_OBJ_VAL((tracker), 0))

void _add_to_initiative_tracker(struct obj_data *tracker, const char *name, int value, vnum_t idnum) {
  // Create the new initiative object.
  struct obj_data *head, *initiative = read_object(OBJ_GRAFFITI, VIRTUAL);

  if (!initiative) {
    mudlog("SYSERR: Unable to create new entry for initiative tracker!", NULL, LOG_SYSLOG, TRUE);
    send_to_room("Something went wrong, and the new tracker entry was not created.\r\n", get_obj_in_room(tracker));
    return;
  }

  GET_TRACKER_ENTRY_INIT_VALUE(initiative) = value;
  GET_TRACKER_ENTRY_PLR_IDNUM(initiative) = idnum;
  initiative->restring = str_dup(name);
  initiative->in_obj = tracker;
  GET_OBJ_EXTRA(initiative).SetBit(ITEM_EXTRA_NORENT);

  // Seek for where we can add it.
  if (!(head = tracker->contains) || GET_TRACKER_ENTRY_INIT_VALUE(head) < GET_TRACKER_ENTRY_INIT_VALUE(initiative)) {
    // There was no head, or the head was lesser to us-- make it the first obj and call it done.
    tracker->contains = initiative;
    initiative->next_content = head;
    return;
  }

  for (struct obj_data *tmp = head->next_content; tmp; tmp = tmp->next_content) {
    // In-place insert.
    if (GET_TRACKER_ENTRY_INIT_VALUE(tmp) <= GET_TRACKER_ENTRY_INIT_VALUE(initiative)) {
      head->next_content = initiative;
      initiative->next_content = tmp;
      return;
    }
    head = head->next_content;
  }

  // We hit the end and had nothing better. Add it at the end.
  head->next_content = initiative;
}

bool _tracker_is_locked_against_ch(struct obj_data *tracker, struct char_data *ch) {
  return GET_TRACKER_LOCK_IDNUM(tracker) && GET_TRACKER_LOCK_IDNUM(tracker) != GET_IDNUM(ch);
}

void _send_tracker_help_to_ch(struct obj_data *tracker, struct char_data *ch) {
  if (!tracker->contains) {
    send_to_char("The initiative tracker hasn't been populated yet. Use ^WTRACKER ROLL^n to begin.\r\n", ch);
  } else {

    bool can_unlock_tracker = _tracker_is_locked_against_ch(tracker, ch) ? (PRF_FLAGGED(ch, PRF_QUESTOR) || access_level(ch, LVL_BUILDER)) : (GET_TRACKER_LOCK_IDNUM(tracker) == GET_IDNUM(ch));
    if (_tracker_is_locked_against_ch(tracker, ch) && !can_unlock_tracker) {
      send_to_char("The tracker is locked-- you can't edit it.\r\n", ch);
      return;
    }

    send_to_char("Your current initiative tracker options are:\r\n", ch);

    send_to_char(" - ^WTRACKER ADD <player name>^n\r\n", ch);
    send_to_char(" - ^WTRACKER ADD <NPC name> <value>^n\r\n", ch);
    send_to_char(" - ^WTRACKER EDIT <existing name> <value>^n\r\n", ch);
    send_to_char(" - ^WTRACKER DELETE <existing name>^n\r\n", ch);
    send_to_char(" - ^WTRACKER ADVANCE^n (to go to the next initiative pass)\r\n", ch);
    send_to_char(" - ^WTRACKER CLEAR^n\r\n", ch);

    // If we've gotten here, the tracker was either locked by ch or wasn't locked at all.
    if (can_unlock_tracker) {
      send_to_char(" - ^WTRACKER UNLOCK^n\r\n", ch);
    } else {
      send_to_char(" - ^WTRACKER LOCK^n\r\n", ch);
    }
  }
}

void _append_tracker_list_to_string(struct obj_data *tracker, char *str, size_t str_size) {
  for (struct obj_data *entry = tracker->contains; entry; entry = entry->next_content) {
    snprintf(ENDOF(str), str_size - strlen(str), "(%2d) ^c%s^n%s\r\n",
             GET_TRACKER_ENTRY_INIT_VALUE(entry),
             GET_OBJ_NAME(entry),
             GET_TRACKER_ENTRY_PLR_IDNUM(entry) ? "" : " (NPC)"
           );
  }
}

void _list_tracker_to_char(struct obj_data *tracker, struct char_data *ch, bool silence_errors) {
  if (!tracker->contains) {
    if (!silence_errors) {
      send_to_char("There isn't an initiative list at the moment.\r\n", ch);
      if (!_tracker_is_locked_against_ch(tracker, ch)) {
        send_to_char("Use ^WTRACKER ROLL^n to create one for the whole room, or add individuals with ^WTRACKER ADD <name> [value]^n.\r\n", ch);
      }
    }
    return;
  }

  strlcpy(tracker_primary_buf, "Current initiative list:\r\n", sizeof(tracker_primary_buf));
  _append_tracker_list_to_string(tracker, tracker_primary_buf, sizeof(tracker_primary_buf));
  send_to_char(tracker_primary_buf, ch);
}

void _generate_new_initiative_track(struct obj_data *tracker, struct char_data *ch) {
  snprintf(tracker_primary_buf, sizeof(tracker_primary_buf), "%s generates a new initiative track.\r\n", GET_CHAR_NAME(ch));
  for (struct char_data *vict = get_ch_in_room(ch)->people; vict; vict = vict->next_in_room) {
    int new_init_value = roll_default_initiative(vict);
    _add_to_initiative_tracker(tracker, GET_CHAR_NAME(vict), new_init_value, GET_IDNUM(vict));
  }

  strlcat(tracker_primary_buf, "\r\nThe new initiative list is:\r\n", sizeof(tracker_primary_buf));
  _append_tracker_list_to_string(tracker, tracker_primary_buf, sizeof(tracker_primary_buf));

  send_to_room(tracker_primary_buf, get_ch_in_room(ch));
}

void _tracker_roll(struct obj_data *tracker, struct char_data *ch, bool reroll) {
  if (tracker->contains) {
    // Only allow 'tracker roll' if everyone is zero.
    for (struct obj_data *entry = tracker->contains; entry; entry = entry->next_content) {
      if (GET_TRACKER_ENTRY_INIT_VALUE(entry) > 0) {
        send_to_char("There are still people with initiative passes remaining."
                     " You can either ^WTRACKER ADVANCE^n until they're all done,"
                     " or ^WTRACKER REROLL^n to reroll everyone anyways.\r\n", ch);
        return;
      }
    }
  }

  snprintf(tracker_primary_buf, sizeof(tracker_primary_buf), "%s starts a new turn on the initiative tracker.\r\n", GET_CHAR_NAME(ch));

  // If it's empty, roll everyone in the room.
  if (!tracker->contains) {
    _generate_new_initiative_track(tracker, ch);
    return;
  }

  // Otherwise, zero out all entries.
  bool list_has_npcs = FALSE;
  for (struct obj_data *entry = tracker->contains; entry; entry = entry->next_content) {
    if (GET_TRACKER_ENTRY_INIT_VALUE(entry) > 0) {
      snprintf(ENDOF(tracker_primary_buf), sizeof(tracker_primary_buf) - strlen(tracker_primary_buf), " - Discarding %s's remaining %d initiative.\r\n", GET_OBJ_NAME(entry), GET_TRACKER_ENTRY_INIT_VALUE(entry));
    }
    GET_TRACKER_ENTRY_INIT_VALUE(entry) = 0;

    if (GET_TRACKER_ENTRY_PLR_IDNUM(entry) == 0)
      list_has_npcs = TRUE;
  }

  // Reroll any players who are still on it.
  for (struct char_data *vict = get_ch_in_room(ch)->people; vict; vict = vict->next_in_room) {
    for (struct obj_data *entry = tracker->contains; entry; entry = entry->next_content) {
      if (GET_TRACKER_ENTRY_PLR_IDNUM(entry) == GET_IDNUM(vict)) {
        // Remove the old entry.
        obj_from_obj(entry);
        extract_obj(entry);

        // Add a new entry.
        int new_init_value = roll_default_initiative(vict);
        _add_to_initiative_tracker(tracker, GET_CHAR_NAME(vict), new_init_value, GET_IDNUM(vict));
        break;
      }
    }
  }

  strlcat(tracker_primary_buf, "\r\nThe new initiative list is:\r\n", sizeof(tracker_primary_buf));
  _append_tracker_list_to_string(tracker, tracker_primary_buf, sizeof(tracker_primary_buf));

  if (list_has_npcs)
    strlcat(tracker_primary_buf, "\r\nNPCs must be rolled manually and set with ^WTRACKER EDIT <name>^n.\r\n", sizeof(tracker_primary_buf));

  send_to_room(tracker_primary_buf, get_ch_in_room(ch));
}

SPECIAL(initiative_tracker)
{
  if (!CMD_IS("track") && !CMD_IS("tracker"))
    return FALSE;

  struct obj_data *tracker = (struct obj_data *) me;

  // Clear the existing lock if the locker is no longer here.
  if (GET_TRACKER_LOCK_IDNUM(tracker)) {
    bool lock_is_valid = FALSE;
    for (struct char_data *vict = get_ch_in_room(ch)->people; vict && !lock_is_valid; vict = vict->next_in_room) {
      lock_is_valid = (!IS_NPC(vict) && GET_IDNUM(vict) == GET_TRACKER_LOCK_IDNUM(tracker));
    }
    if (!lock_is_valid) {
      GET_TRACKER_LOCK_IDNUM(tracker) = 0;
      send_to_char("The tracker unlocks due to the locker no longer being present.\r\n", ch);
    }
  }

  if (CMD_IS("track") || CMD_IS("tracker")) {
    char name[MAX_INPUT_LENGTH];

    char *remainder = any_one_arg(argument, arg);

    if (!*arg) {
      _list_tracker_to_char(tracker, ch, FALSE);
      bool can_unlock_tracker = _tracker_is_locked_against_ch(tracker, ch) ? (PRF_FLAGGED(ch, PRF_QUESTOR) || access_level(ch, LVL_BUILDER)) : (GET_TRACKER_LOCK_IDNUM(tracker) == GET_IDNUM(ch));
      if (!_tracker_is_locked_against_ch(tracker, ch) || can_unlock_tracker) {
        send_to_char("\r\nUse ^WTRACKER HELP^n to see available commands.\r\n", ch);
      }
      return TRUE;
    }

    if (is_abbrev(arg, "lock") || is_abbrev(arg, "unlock")) {
      // Run this just to unlock it if needed.
      _tracker_is_locked_against_ch(tracker, ch);

      if (is_abbrev(arg, "lock")) {
        // Attempt to override a tracker locked by someone else.
        if (GET_TRACKER_LOCK_IDNUM(tracker)) {
          if (GET_TRACKER_LOCK_IDNUM(tracker) != GET_IDNUM(ch)) {
            if (PRF_FLAGGED(ch, PRF_QUESTOR) || access_level(ch, LVL_BUILDER)) {
              snprintf(tracker_primary_buf, sizeof(tracker_primary_buf), "%s overrides and re-locks the initiative tracker.\r\n", GET_CHAR_NAME(ch));
              send_to_room(tracker_primary_buf, get_ch_in_room(ch));
              GET_TRACKER_LOCK_IDNUM(tracker) = GET_IDNUM(ch);
            } else {
              send_to_char("The tracker has been locked by someone else.\r\n", ch);
            }
          } else {
            send_to_char("The tracker is already locked.\r\n", ch);
          }
          return TRUE;
        }

        // Add your own lock.
        else {
          GET_TRACKER_LOCK_IDNUM(tracker) = GET_IDNUM(ch);
          snprintf(tracker_primary_buf, sizeof(tracker_primary_buf), "%s locks the initiative tracker.\r\n", GET_CHAR_NAME(ch));
          send_to_room(tracker_primary_buf, get_ch_in_room(ch));
          return TRUE;
        }
      }

      // Attempt to unlock the tracker.
      else {
        if (GET_TRACKER_LOCK_IDNUM(tracker)) {
          if (GET_TRACKER_LOCK_IDNUM(tracker) != GET_IDNUM(ch)) {
            if (PRF_FLAGGED(ch, PRF_QUESTOR) || access_level(ch, LVL_BUILDER)) {
              snprintf(tracker_primary_buf, sizeof(tracker_primary_buf), "%s overrides and unlocks the initiative tracker.\r\n", GET_CHAR_NAME(ch));
              send_to_room(tracker_primary_buf, get_ch_in_room(ch));
              GET_TRACKER_LOCK_IDNUM(tracker) = 0;
            } else {
              send_to_char("The tracker has been locked by someone else.\r\n", ch);
            }
            return TRUE;
          }
          // Add your own lock.
          else {
            GET_TRACKER_LOCK_IDNUM(tracker) = 0;
            snprintf(tracker_primary_buf, sizeof(tracker_primary_buf), "%s unlocks the initiative tracker.\r\n", GET_CHAR_NAME(ch));
            send_to_room(tracker_primary_buf, get_ch_in_room(ch));
            return TRUE;
          }
        } else {
          send_to_char("The tracker is not locked.\r\n", ch);
          return TRUE;
        }
      }
    }

    remainder = any_one_arg(remainder, name);
    struct obj_data *entry_in_tracker = _find_name_in_tracker(tracker, name, TRUE);

    if (is_abbrev(arg, "add")) {
      struct char_data *vict = NULL;

      if (_tracker_is_locked_against_ch(tracker, ch)) {
        send_to_char("The tracker has been locked-- you can't modify it.\r\n", ch);
        return TRUE;
      }

      if (!str_cmp(name, "self") || !str_cmp(name, "me")) {
        vict = ch;
      }

      if (entry_in_tracker) {
        send_to_char(ch, "%s is already in the tracker. Use ^WTRACKER EDIT <name> <number>^n to alter their values.\r\n", GET_OBJ_NAME(entry_in_tracker));
        return TRUE;
      }

      // Are we targeting a player?
      for (vict = get_ch_in_room(ch)->people; vict; vict = vict->next_in_room) {
        if (IS_NPC(vict))
          continue;

        if (!str_cmp(name, GET_CHAR_NAME(vict)))
          break;
        if (!*remainder && is_abbrev(name, GET_CHAR_NAME(vict)))
          send_to_char(ch, "(If you're trying to target %s, you'll have to use ^WTRACKER ADD %s^n.)\r\n", GET_CHAR_NAME(vict), GET_CHAR_NAME(vict));
      }

      int new_init_value;
      if (!*remainder || (new_init_value = atoi(remainder)) <= 0) {
        if (!vict) {
          send_to_char(ch, "You must specify a value for someone who's not here (e.g. ^WTRACKER ADD %s 12^n)\r\n", *name ? name : "GROG");
          return TRUE;
        } else {
          new_init_value = roll_default_initiative(vict);
          snprintf(tracker_primary_buf, sizeof(tracker_primary_buf), "%s auto-rolled and added %s (%d) to the initiative tracker.", GET_CHAR_NAME(ch), GET_CHAR_NAME(vict), new_init_value);
          strlcpy(name, GET_CHAR_NAME(vict), sizeof(name));
        }
      } else {
        if (vict) {
          snprintf(tracker_primary_buf, sizeof(tracker_primary_buf), "%s added %s with manually-set initiative %d to the tracker.", GET_CHAR_NAME(ch), GET_CHAR_NAME(vict), new_init_value);
          strlcpy(name, GET_CHAR_NAME(vict), sizeof(name));
        } else {
          snprintf(tracker_primary_buf, sizeof(tracker_primary_buf), "%s added NPC %s (%d) to the initiative tracker.\r\n", GET_CHAR_NAME(ch), CAP(name), new_init_value);
        }
      }
      send_to_room(tracker_primary_buf, get_ch_in_room(ch));

      // Add it to the initiative list.
      _add_to_initiative_tracker(tracker, CAP(name), new_init_value, vict ? GET_IDNUM(vict) : 0);

      return TRUE;
    }

    if (is_abbrev(arg, "delete") || is_abbrev(arg, "remove") || is_abbrev(arg, "drop")) {
      if (_tracker_is_locked_against_ch(tracker, ch)) {
        send_to_char("The tracker has been locked-- you can't modify it.\r\n", ch);
        return TRUE;
      }

      if (!entry_in_tracker) {
        send_to_char(ch, "Couldn't find an entry named %s in the tracker. Exact names only, please.\r\n", name);
        return TRUE;
      }

      obj_from_obj(entry_in_tracker);
      snprintf(tracker_primary_buf, sizeof(tracker_primary_buf), "%s removed %s%s (%d) from the initiative tracker.\r\n",
               GET_CHAR_NAME(ch),
               GET_TRACKER_ENTRY_PLR_IDNUM(entry_in_tracker) ? "" : "NPC ",
               GET_OBJ_NAME(entry_in_tracker),
               GET_TRACKER_ENTRY_INIT_VALUE(entry_in_tracker));
      send_to_room(tracker_primary_buf, get_ch_in_room(ch));
      extract_obj(entry_in_tracker);
      return TRUE;
    }

    if (is_abbrev(arg, "edit") || is_abbrev(arg, "set") || is_abbrev(arg, "modify") || is_abbrev(arg, "configure")) {
      if (_tracker_is_locked_against_ch(tracker, ch)) {
        send_to_char("The tracker has been locked-- you can't modify it.\r\n", ch);
        return TRUE;
      }

      if (!entry_in_tracker) {
        send_to_char(ch, "Couldn't find an entry named %s in the tracker. Exact names only, please.\r\n", name);
        return TRUE;
      }

      int new_init_value;
      if (!*remainder || (new_init_value = atoi(remainder)) < 0) {
        send_to_char("New initiative value must be 0 or higher.\r\n", ch);
        return TRUE;
      }

      snprintf(tracker_primary_buf, sizeof(tracker_primary_buf), "%s updated %s%s (%d -> %d) in the initiative tracker.\r\n",
               GET_CHAR_NAME(ch),
               GET_TRACKER_ENTRY_PLR_IDNUM(entry_in_tracker) ? "" : "NPC ",
               GET_OBJ_NAME(entry_in_tracker),
               GET_TRACKER_ENTRY_INIT_VALUE(entry_in_tracker),
               new_init_value);
      send_to_room(tracker_primary_buf, get_ch_in_room(ch));

      // Add the new entry to the tracker.
      _add_to_initiative_tracker(tracker, GET_OBJ_NAME(entry_in_tracker), new_init_value, GET_TRACKER_ENTRY_PLR_IDNUM(entry_in_tracker));

      // Rip out the old one.
      obj_from_obj(entry_in_tracker);
      extract_obj(entry_in_tracker);
      return TRUE;
    }

    if (is_abbrev(arg, "list") || is_abbrev(arg, "status")) {
      _list_tracker_to_char(tracker, ch, FALSE);
      return TRUE;
    }

    if (is_abbrev(arg, "help") || is_abbrev(arg, "info") || is_abbrev(arg, "guide")) {
      _send_tracker_help_to_ch(tracker, ch);
      return TRUE;
    }

    if (is_abbrev(arg, "clear")) {
      if (_tracker_is_locked_against_ch(tracker, ch)) {
        send_to_char("The tracker has been locked-- you can't modify it.\r\n", ch);
        return TRUE;
      }

      if (!tracker->contains) {
        send_to_char(ch, "The initiative tracker is already empty.\r\n");
        return TRUE;
      }

      char current_init_list[MAX_STRING_LENGTH];
      bool printed_anything = FALSE;
      snprintf(current_init_list, sizeof(current_init_list), "%s cleared the initiative list, which held: ", GET_CHAR_NAME(ch));
      for (struct obj_data *entry = tracker->contains; entry; entry = entry->next_content) {
        snprintf(ENDOF(current_init_list), sizeof(current_init_list) - strlen(current_init_list), "%s%s (%d)",
                 printed_anything ? ", " : "",
                 GET_OBJ_NAME(entry),
                 GET_TRACKER_ENTRY_INIT_VALUE(entry));
        printed_anything = TRUE;
      }
      if (!printed_anything)
        strlcat(current_init_list, "nothing.", sizeof(current_init_list));

      strlcat(current_init_list, "\r\n", sizeof(current_init_list));
      send_to_room(current_init_list, get_ch_in_room(ch));

      while (tracker->contains) {
        struct obj_data *tmp = tracker->contains;
        obj_from_obj(tmp);
        extract_obj(tmp);
      }
      return TRUE;
    }

    if (is_abbrev(arg, "advance") || is_abbrev(arg, "next")) {
      if (_tracker_is_locked_against_ch(tracker, ch)) {
        send_to_char("The tracker has been locked-- you can't modify it.\r\n", ch);
        return TRUE;
      }

      if (!tracker->contains) {
        send_to_char(ch, "There's nothing to advance. Start a new list with ^WTRACKER ROLL^n.\r\n");
        return TRUE;
      }

      snprintf(tracker_primary_buf, sizeof(tracker_primary_buf), "%s advanced the initiative list to the next pass.\r\n", GET_CHAR_NAME(ch));
      send_to_room(tracker_primary_buf, get_ch_in_room(ch));

      bool have_someone_left = FALSE;

      struct obj_data *next;
      for (struct obj_data *entry = tracker->contains; entry; entry = next) {
        next = entry->next_content;
        if (GET_TRACKER_ENTRY_INIT_VALUE(entry) > 0) {
          GET_TRACKER_ENTRY_INIT_VALUE(entry) -= 10;
          if (GET_TRACKER_ENTRY_INIT_VALUE(entry) <= 0) {
            snprintf(tracker_primary_buf, sizeof(tracker_primary_buf), " - %s%s is out of initiative.\r\n",
                     GET_TRACKER_ENTRY_PLR_IDNUM(entry) ? "" : "NPC ",
                     GET_OBJ_NAME(entry));
            send_to_room(tracker_primary_buf, get_ch_in_room(ch));
            GET_TRACKER_ENTRY_INIT_VALUE(entry) = 0;
          } else {
            have_someone_left = TRUE;
          }
        }
      }

      send_to_room("\r\n", get_ch_in_room(ch));

      if (!have_someone_left) {
        _tracker_roll(tracker, ch, FALSE);
        return TRUE;
      }

      strlcpy(tracker_primary_buf, "Current initiative list is now:\r\n", sizeof(tracker_primary_buf));
      _append_tracker_list_to_string(tracker, tracker_primary_buf, sizeof(tracker_primary_buf));

      send_to_room(tracker_primary_buf, get_ch_in_room(ch));

      return TRUE;
    }

    if (is_abbrev(arg, "reroll") || is_abbrev(arg, "roll") || is_abbrev(arg, "start") || is_abbrev(arg, "initialize")) {
      if (_tracker_is_locked_against_ch(tracker, ch)) {
        send_to_char("The tracker has been locked-- you can't modify it.\r\n", ch);
        return TRUE;
      }

      _tracker_roll(tracker, ch, !str_cmp(arg, "reroll"));
      return TRUE;
    }

    _send_tracker_help_to_ch(tracker, ch);
    return TRUE;
  }

  return FALSE;
}
#undef GET_TRACKER_ENTRY_INIT_VALUE
#undef GET_TRACKER_ENTRY_PLR_IDNUM
#undef GET_TRACKER_LOCK_IDNUM
