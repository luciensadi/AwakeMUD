/* Channels:

- Are public (can be found in a directory) or private (join by invite from someone in the channel only)
- Are administered by staff (set name, shortname, desc, public/private)
- Can have moderators who can invite / kick? idk
- Have a history that can be looked up by anyone on the channel, whether they were online or not

imp details:
- when a command is not recognized, only then run it through the channel names/aliases that the person is part of: channels always subordinate to commands
- need two tables: channel (id, name, etc) and moderator (channel id, player id)
*/

#include <unordered_map>

#include "structs.hpp"
#include "db.hpp"
#include "interpreter.hpp"
#include "moderation.hpp"
#include "channels.hpp"

std::unordered_map<idnum_t, CommsChannel *> comms_channel_map = {};

// Class methods.
bool CommsChannel::ch_is_member(struct char_data *ch) {
  // TODO
  return FALSE;
}

bool CommsChannel::attempt_send_message(struct char_data *ch, char *msg) {
  char cmd[MAX_INPUT_LENGTH];
  char *message = any_one_arg(msg, cmd);

  // Require that we're either an abbreviation of the channel name or an exact match to the channel alias.
  if (!is_abbrev(cmd, name) && str_cmp(cmd, alias)) {
    return FALSE;
  }

  // Require that we're a member of the channel.
  if (!ch_is_member(ch)) {
    return FALSE;
  }

  // Check for profanity. Returns TRUE because it hit a valid channel, just had an unusable message.
  if (check_for_banned_content(message, ch)) {
    return TRUE;
  }

  char composed_message[MAX_INPUT_LENGTH + 50];
  snprintf(composed_message, sizeof(composed_message), 
           "%s%s%s %s %s%s%s%s%s\"",
           pre_name, GET_CHAR_NAME(ch), post_name,   // [Lucien]
           slug,                                     // (OOC),
           color_code, quoted_message ? "\"" : "",   // ^n"
           message,                                  // Vile is amazing
           color_code, quoted_message ? "\"" : "");  // ^n"

  // Send our composed message.
  for (struct descriptor_data *d = descriptor_list; d; d = d->next) {
    if (d->character && ch_is_member(d->character)) {
      send_to_char(composed_message, d->character);
    }
  }

  // Save it to history (max 100 lines)
  history.push_back(composed_message);
  if (history.size() > 100) {
    history.erase(history.begin());
  }

  return TRUE;
}

// End class methods.

// TODO: Command to look up channel history (integrate with existing history command?)

// TODO: Load channels from DB.

// TODO: Create/edit channel and save to DB.

// Message sending hook for interpreter.cpp.
bool send_command_as_custom_channel_message(struct char_data *ch, char *arg) {
  // Since the channel command comes before the frozen check, we have to re-add the check here.
  if (PLR_FLAGGED(ch, PLR_FROZEN)) {
    if (!access_level(ch, LVL_PRESIDENT)) {
      send_to_char("Sorry, this character has been frozen by staff and is unable to take any input. If you're seeing this message, it usually means that you've connected to a character that is pending review or has been banned.\r\n", ch);
      return TRUE;
    } else
      send_to_char("The ice covering you crackles alarmingly as you slam your sovereign will through it.\r\n", ch);
  }

  // Check to see if the command matches a channel.
  for (const std::pair<const idnum_t, CommsChannel *> & chan_pair : comms_channel_map) {
    if (chan_pair.second->attempt_send_message(ch, arg))
      return TRUE;
  }

  // No match, bail out.
  return FALSE;
}