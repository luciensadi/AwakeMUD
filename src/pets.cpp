/* I'm going to regret doing this, aren't I?

  PETS are objects that wander when placed in an apartment.
  - made with CREATE PET like art; customize it and pay XX syspoints
    - name, shortdesc, room desc
    - which set of premade echoes you want it to use
  - cannot be dropped outside of apartments, but can be held
  - moves slowly around the apartment, will not leave (gag with movegag)
  - does environmental echoes rarely

  STRETCH: It moves around. Means you have to do saving etc of apartments on a somewhat regular basis. Think it through.

  STRETCH / v2: custom echoes
  - premade echoes are an unordered map: there's an idnum for the echo set, a name, an author idnum, a private y/n toggle, and a vector of echo messages
  - you can create a new set of echoes for XXX sysp, and specify if only you can work with them, or if they're public for use (private costs more)
  - you can edit your echoes for X sysp
*/

#include <map>
#include <unordered_map>

#include "awake.hpp"
#include "structs.hpp"
#include "olc.hpp"
#include "db.hpp"
#include "handler.hpp"
#include "pets.hpp"

#define CH d->character
#define PET d->edit_obj

std::unordered_map<idnum_t, class PetEchoSet> pet_echo_sets = {
  {1, {0, TRUE, "Cat, Lazy", "", "", {
        "$p pads over to a warm spot and sprawls out contentedly.",
        "$p streeeetches.",
        "A low, rumbling purr comes from $p."
      }}},
  {2, {0, TRUE, "Cat, Playful", "", "", {
        "$p bounds past, chasing a toy.",
        "$p gets distracted by a bird and starts chattering back at it.",
        "$p bats curiously at a dangling bit of something."
      }}},
  {3, {0, TRUE, "Cat, Cuddly", "", "", {
        "$p wanders over, seeking a lap.",
        "$p purrs quietly, just happy to be included.",
        "$p strolls by, giving you a soft head-bonk as it goes."
      }}},

  {4, {0, TRUE, "Dog, Lazy", "", "", {
        "$p pads over to a warm spot and sprawls out contentedly.",
        "$p streeeetches.",
        "There's a quiet thumping as $p's tail starts to wag."
      }}},
  {5, {0, TRUE, "Dog, Playful", "", "", {
        "$p bounds past, chasing a toy.",
        "$p romps around happily.",
        "$p gets into something it probably shouldn't."
      }}},
  {6, {0, TRUE, "Dog, Cuddly", "", "", {
        "$p wanders over, seeking someone to flop against.",
        "$p huffs and rolls over to rest its head against the nearest person.",
        "$p wags its tail as it looks at you."
      }}},

  {7, {0, TRUE, "Sleepy Pet", "", "", {
        "$p rolls over in its sleep.",
        "$p yawns widely before settling back in for another nap.",
        "$p twitches as it dreams."
      }}},
};

std::map<std::string, idnum_t> pet_echo_sets_by_name = {};
void alphabetize_pet_echoes_by_name() {
  for (auto &it : pet_echo_sets) {
    pet_echo_sets_by_name[it.second.get_name()] = it.first;
  }
}

PetEchoSet *get_pet_echo_set(idnum_t idnum) {
  std::unordered_map<idnum_t, class PetEchoSet>::iterator found;

  if ((found = pet_echo_sets.find(idnum)) != pet_echo_sets.end()) {
    return &(found->second);
  }
  return NULL;
}

void pet_acts(struct obj_data *pet) {
  if (!pet->in_room || !pet->in_room->people || !pet->in_room->apartment)
    return;

  class PetEchoSet *selected_echo_set = get_pet_echo_set(GET_PET_ECHO_SET_IDNUM(pet));

  if (!selected_echo_set) {
    mudlog_vfprintf(NULL, LOG_SYSLOG, "SYSERR: Hit pet_acts(%s) with pet that had echo_set %d.", GET_PET_ECHO_SET_IDNUM(pet));
    return;
  }

  act(selected_echo_set->get_random_echo_message(), FALSE, NULL, pet, NULL, TO_ROOM);
}

void create_pet_main_menu(struct descriptor_data *d) {
  class PetEchoSet *selected_echo_set = get_pet_echo_set(GET_PET_ECHO_SET_IDNUM(PET));

  CLS(CH);
  send_to_char(CH, "Welcome to pet creation. ^WNo ASCII art, please.^n\r\n");
  send_to_char(CH, "1) ^cShort Desc: ^n%s^n\r\n", GET_OBJ_NAME(PET));
  send_to_char(CH, "2) ^cDescription: ^n%s^n\r\n", PET->photo ? PET->photo : "^y<not set>");
  send_to_char(CH, "3) ^cRoom Description: ^n%s^n\r\n", PET->graffiti ? PET->graffiti : "^y<not set>");

  send_to_char(CH, "4) ^cFlavor Message Set: ^n%s^n\r\n", !selected_echo_set ? "^y<not set>^n" : selected_echo_set->get_name());
  if (selected_echo_set) {
#ifdef PETS_MOVE_AROUND
    send_to_char(CH, "  - Arriving: ^n%s %s north.^n\r\n", CAP(GET_OBJ_NAME(PET)), selected_echo_set->get_arrive());
    send_to_char(CH, "  - Leaving:  ^n%s %s north.^n\r\n", CAP(GET_OBJ_NAME(PET)), selected_echo_set->get_leave());
#endif
    send_to_char(CH, "  - Echoes: \r\n");
    for (auto message : selected_echo_set->get_environmental_messages()) {
      send_to_char("      ", CH);
      act(message, FALSE, CH, PET, 0, TO_CHAR | TO_SLEEP);
    }
  }

  send_to_char(CH, "q) ^cSave and Quit^n\r\n");
  send_to_char(CH, "x) ^cDiscard and Exit^n (refunds ^c%d^n syspoints)\r\n", CUSTOM_PET_SYSPOINT_COST);
  send_to_char(CH, "Enter Option: ");

  STATE(d) = CON_PET_CREATE;
  d->edit_mode = PET_EDIT_MAIN;
}

void create_pet_parse(struct descriptor_data *d, const char *arg) {
  switch(d->edit_mode) {
    case PET_EDIT_MAIN:
      switch (*arg) {
        case '1':
          {
            send_to_char(CH, "Enter pet's short description (e.g. 'a tiny chihuahua'): ");
            d->edit_mode = PET_EDIT_NAME;
          }
          break;
        case '2':
          {
            send_to_char(CH, "Enter pet's look description:\r\n");
            d->edit_mode = PET_EDIT_DESC;
            DELETE_D_STR_IF_EXTANT(d);
            INITIALIZE_NEW_D_STR(d);
            d->max_str = MAX_MESSAGE_LENGTH;
            d->mail_to = 0;
          }
          break;
        case '3':
          {
            send_to_char(CH, "Enter pet's room description (e.g. 'A tiny chihuahua sits here, trembling with rage.'): ");
            d->edit_mode = PET_EDIT_ROOMDESC;
          }
          break;
        case '4':
          {
            send_to_char(CH, "The following flavor message sets are available:\r\n");
            int idx = 1;
            for (auto &it : pet_echo_sets_by_name) {
              if (!pet_echo_sets.at(it.second).is_usable_by(GET_IDNUM_EVEN_IF_PROJECTING(CH)))
                continue;
              
              send_to_char(CH, "%2d) %s\r\n", idx++, it.first.c_str());
            }
            send_to_char(CH, "\r\nSelect a number, or enter 0 to abort: ");
            d->edit_mode = PET_EDIT_FLAVOR_MESSAGES;
          }
          break;
        case 'q':
        case 'Q':
          {
            if (!PET->graffiti || !PET->restring || !str_cmp(GET_OBJ_NAME(PET), "a custom pet") || GET_PET_ECHO_SET_IDNUM(PET) == 0) {
              send_to_char("Please finish editing your pet. It's not editable once saved.\r\n", CH);
              return;
            }

            GET_OBJ_EXTRA(PET).SetBit(ITEM_EXTRA_KEPT);
            obj_to_char(PET, CH);
            mudlog_vfprintf(CH, LOG_GRIDLOG, "%s created a custom pet named '%s'.", GET_CHAR_NAME(CH), GET_OBJ_NAME(PET));
            log_vfprintf("Pet desc: '''%s'''\r\nPet room desc: '''%s'''", PET->photo ? PET->photo : "<default>", PET->graffiti ? PET->graffiti : "<default>");
            PET = NULL;
            STATE(d) = CON_PLAYING;
            send_to_char(CH, "Saving custom pet.\r\n");
          }
          break;
        case 'x':
        case 'X':
          {
            GET_SYSTEM_POINTS(CH) += CUSTOM_PET_SYSPOINT_COST;
            extract_obj(PET);
            PET = NULL;
            STATE(d) = CON_PLAYING;
            send_to_char(CH, "OK, discarded changes and refunded you %d syspoints.\r\n", CUSTOM_PET_SYSPOINT_COST);
          }
          break;
      }
      break;
    case PET_EDIT_NAME:
    case PET_EDIT_ROOMDESC:
      {
        int length_with_no_color = get_string_length_after_color_code_removal(arg, CH);

        // Silent failure: We already sent the error message in get_string_length_after_color_code_removal().
        if (length_with_no_color == -1) {
          create_pet_main_menu(d);
          return;
        }
        if (length_with_no_color >= LINE_LENGTH) {
          send_to_char(CH, "That string is too long, please shorten it. The maximum length after color code removal is %d characters.\r\n", LINE_LENGTH - 1);
          create_pet_main_menu(d);
          return;
        }

        if (strlen(arg) >= MAX_RESTRING_LENGTH) {
          send_to_char(CH, "That string is too long, please shorten it. The maximum length with color codes included is %d characters.\r\n", MAX_RESTRING_LENGTH - 1);
          create_pet_main_menu(d);
          return;
        }

        if (d->edit_mode == PET_EDIT_NAME) {
          DELETE_ARRAY_IF_EXTANT(PET->restring);
          PET->restring = str_dup(arg);
        } else {
          char replaced_colors[MAX_INPUT_LENGTH + 6];
          snprintf(replaced_colors, sizeof(replaced_colors), "^n%s^n", arg);
          DELETE_ARRAY_IF_EXTANT(PET->graffiti);
          PET->graffiti = str_dup(replaced_colors);
        }
        
        create_pet_main_menu(d);
      }
      break;
    case PET_EDIT_FLAVOR_MESSAGES:
      {
        int number = atoi(arg);
        if (number <= 0) {
          create_pet_main_menu(d);
          return;
        }

        int idx = 1;
        for (auto &it : pet_echo_sets_by_name) {
          if (!pet_echo_sets.at(it.second).is_usable_by(GET_IDNUM_EVEN_IF_PROJECTING(CH)))
            continue;

          if (idx++ == number) {
            GET_PET_ECHO_SET_IDNUM(PET) = it.second;
            create_pet_main_menu(d);
            return;
          }
        }

        send_to_char(CH, "%d doesn't correspond to a flavor message set. Select a valid one, or enter 0 to cancel: ", number);
      }
      break;
  }
}

void create_pet(struct char_data *ch) {
  FAILURE_CASE(PLR_FLAGGED(ch, PLR_BLACKLIST), "You can't do that while blacklisted.");
  FAILURE_CASE_PRINTF(GET_SYSTEM_POINTS(ch) < CUSTOM_PET_SYSPOINT_COST, "It will cost you %d syspoints to create a custom pet.", CUSTOM_PET_SYSPOINT_COST);
  GET_SYSTEM_POINTS(ch) -= CUSTOM_PET_SYSPOINT_COST;
  send_to_char(ch, "\r\nYou spend %d syspoints, which will be refunded if you 'X' out.\r\n\r\n", CUSTOM_PET_SYSPOINT_COST);
  // If someone disconnects during editing, they'll lose the points and will need to ask staff for a refund.

  ch->desc->edit_obj = read_object(OBJ_CUSTOM_PET, VIRTUAL, OBJ_LOAD_REASON_CREATE_PET);
  GET_PET_OWNER_IDNUM(ch->desc->edit_obj) = GET_IDNUM_EVEN_IF_PROJECTING(ch);
  GET_PET_ECHO_SET_IDNUM(ch->desc->edit_obj) = 0;

  create_pet_main_menu(ch->desc);
}

void debug_pet_menu(struct char_data *ch) {
  send_to_char(ch, "buckle up bitch\r\n");

  create_pet(ch);
  send_to_char("\r\nsyke\r\n", ch);
  extract_obj(ch->desc->edit_obj);
  ch->desc->edit_obj = NULL;
  STATE(ch->desc) = CON_PLAYING;
  ch->desc->edit_mode = 0;
}