/* Sound prop:
 * - Provide a list of sound strings and the length of that list
 * - Starting from rooms surrounding you, the strings will be printed to the rooms
 */

#include <vector>
#include <unordered_map>

#include "awake.hpp"
#include "structs.hpp"
#include "comm.hpp"
#include "db.hpp"
#include "ignore_system.hpp"

extern void combat_message_process_single_ranged_response(struct char_data *ch, struct char_data *tch);

void propagate_sound_from_room(struct room_data *original_room, std::vector<const char *> sound_vect, bool is_combat_noise, struct char_data *originator) {
  std::vector<struct room_data *> evaluate_list = {};
  std::vector<struct room_data *> next_tier_list = {};

  // Mark the original room as visited.
  ROOM_FLAGS(original_room).SetBit(ROOM_BFS_MARK);

  // Pre-populate our evaluate list: We don't want to print to the original room, but want to print to its exits.
  for (int dir = 0; dir < NUM_OF_DIRS; dir++) {
    // Skip non-existent and silenced rooms.
    if (!original_room->dir_option[dir]
        || !original_room->dir_option[dir]->to_room
        || original_room->dir_option[dir]->to_room->silence[ROOM_NUM_SPELLS_OF_TYPE] > 0)
    {
      continue;
    }

    // It's valid-- add it to our list.
    evaluate_list.emplace_back(original_room->dir_option[dir]->to_room);
  }

  while (!sound_vect.empty()) {
    // Iterate over all rooms in our evaluate list.
    for (auto room = evaluate_list.begin(); room != evaluate_list.end(); ++room) {
      // Skip rooms we've visited already.
      if (ROOM_FLAGGED(*room, ROOM_BFS_MARK))
        continue;

      // Mark this room as visited.
      ROOM_FLAGS(*room).SetBit(ROOM_BFS_MARK);

      // Message all characters in the room.
      for (struct char_data *listener = (*room)->people; listener; listener = listener->next_in_room) {
        // Skip people who have opted out of combat noise.
        if (is_combat_noise && PRF_FLAGGED(listener, PRF_FIGHTGAG))
          continue;

        // You don't get noise from people you're ignoring.
        if (IS_IGNORING(listener, is_blocking_ic_interaction_from, originator))
          continue;

        // Send the message.
        send_to_char(sound_vect.front(), listener);

        // NPCs react to hearing combat messages.
        if (is_combat_noise && IS_NPC(listener)) {
          combat_message_process_single_ranged_response(originator, listener);
        }
      }

      // Iterate over all exits from this room.
      for (int dir = 0; dir < NUM_OF_DIRS; dir++) {
        // Skip non-existent and silenced rooms.
        if (!(*room)->dir_option[dir] || !(*room)->dir_option[dir]->to_room || (*room)->dir_option[dir]->to_room->silence[ROOM_NUM_SPELLS_OF_TYPE] > 0)
          continue;

        // Add it to our next list.
        next_tier_list.emplace_back((*room)->dir_option[dir]->to_room);
      }
    }

    // Now that we've sent a sound to all candidate rooms, remove it from the list.
    sound_vect.erase(sound_vect.begin());

    // Replace our current list with our next list.
    evaluate_list = next_tier_list;
    next_tier_list = std::vector<struct room_data *>();
  }

  // Remove all BFS flags from the game.
  for (int i = 0; i <= top_of_world; i++) {
    ROOM_FLAGS(&world[i]).RemoveBit(ROOM_BFS_MARK);
  }
}
