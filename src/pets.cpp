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
#include "utils.hpp"
#include "moderation.hpp"
#include "pets.hpp"

#define CH d->character
#define PET d->edit_obj

struct char_data *dummy_mob_for_pet_pronouns = NULL;

std::unordered_map<idnum_t, class PetEchoSet> pet_echo_sets = {
  {1, {0, TRUE, "Cat, Lazy", "", "", {
        "$p pads over to a warm spot and sprawls out contentedly.",
        "$p streeeetches.",
        "A low, rumbling purr comes from $p.",
        "$p stretches lazily before jumping onto something to explore.",
        "$p flops down on somethng soft."
      }}},
  {2, {0, TRUE, "Cat, Playful", "", "", {
        "$p bounds past, chasing a toy.",
        "$p gets distracted by a bird and starts chattering back at it.",
        "$p bats curiously at a dangling bit of something.",
        "$p gently swats at floating dust specks in the light.",
        "$p prowls around the room, inspecting every nook and cranny."
      }}},
  {3, {0, TRUE, "Cat, Cuddly", "", "", {
        "$p wanders over, seeking a lap.",
        "$p purrs quietly, just happy to be included.",
        "$p strolls by, giving $n a soft head-bonk as $E goes.",
        "$p purrs contentedly as $E curls up beside $n.",
        "$p pushes $S face up under $q hand, demanding pets."
      }}},
  {4, {0, TRUE, "Cat, Friendly", "", "", {
        "$p rubs against $q legs, seeking attention.",
        "$p gives a happy chirrup, pleased to have someone around to cadge petting from.",
        "$p softly meows, wanting to be picked up and petted.",
        "$p rolls over, inviting a belly rub.",
        "$p playfully swats at $q hand, eager for a game."
      }}},
  {5, {0, TRUE, "Cat, Curious", "", "", {
        "$p pokes $S head into an open container, investigating what's inside.",
        "$p leaps up onto a shelf, sniffing around the books and objects.",
        "$p taps at a lightswitch with its paw, trying to figure out how it works.",
        "$p paws at a closed cupboard, trying to get inside.",
        "$p wanders around, meowing at random things."
      }}},

  {101, {0, TRUE, "Dog, Lazy", "", "", {
        "$p pads over to a warm spot and sprawls out contentedly.",
        "$p streeeetches.",
        "There's a quiet thumping as $p's tail starts to wag.",
        "$p rolls over lazily, exposing $S belly.",
        "$p startles at something, then yawns and lays back down."
      }}},
  {102, {0, TRUE, "Dog, Playful", "", "", {
        "$p bounds past, chasing a toy.",
        "$p romps around happily.",
        "$p gets into something $E probably shouldn't.",
        "$p drops a ball in $q lap, demanding playtime.",
        "$p rolls over and wriggles, happy to have someone around."
      }}},
  {103, {0, TRUE, "Dog, Cuddly", "", "", {
        "$p wanders over, seeking someone to flop against.",
        "$p huffs and rolls over to rest $S head against $q leg.",
        "$p wags $S tail as it looks at $n.",
        "$p finds an opportune time to start licking $n.",
        "$p gives $n a pleading look-- time for more petting?"
      }}},
  {104, {0, TRUE, "Dog, Friendly", "", "", {
        "$p wags $S tail happily.",
        "$p nudges your hand, asking for a pet.",
        "$p leaps excitedly at the sound of a doorbell somewhere nearby.",
        "$p trots over, offering $n $S favorite squeaky toy.",
        "$p follows $n around, hoping for a treat."
      }}},
  {105, {0, TRUE, "Dog, Curious", "", "", {
        "$p sniffs every corner, investigating a new scent in the air.",
        "$p tilts $R head, studying an unfamiliar object on the floor.",
        "$p finds a new spot to stick $R nose in and investigate.",
        "$p watches $n closely, eager to see what's going on.",
        "$p tilts $R head inquisitively.",
        "$p wags happily."
      }}},

  {200, {0, TRUE, "Sleepy Pet", "", "", {
        "$p rolls over in $S sleep.",
        "$p yawns widely before settling back in for another nap.",
        "$p twitches as $E dreams.",
        "$p nestles into a blanket, curling up for warmth.",
        "$p stretches lazily, then relaxes back into a deep, restful slumber.",
        "$p curls up in a sunny spot, content to nap the day away.",
        "$p does a biiiiig stretch."
      }}},

  {301, {0, TRUE, "Bunny, Bouncy", "", "", {
        "$p bounces and zooms around the room!",
        "$p suddenly flops to $S side, eyeing $n.",
        "$p attempts to break $S highjump record!",
        "Thump. Thump. Sounds like $p wants more attention from $n.",
        "$p munches on some alfalfa, resting for a moment."
      }}},
  
  {401, {0, TRUE, "Bird, Sleepy", "", "", {
        "$p puffs up $S feathers to keep $Mself warm during $S nap.",
        "$p opens $S beak wide and gives a mighty little yawn.",
        "$p finds $Melf a high perch to snooze securely upon.",
        "$p tucks $S head underneath a wing as $E settles in for a good nap.",
        "$p slowly tilts forward before correcting $Mself as $E begins to nod off."
      }}},
  
  {402, {0, TRUE, "Bird, Lazy", "", "", {
        "$p keeps one eye open as $E settles in for a half-nap.",
        "$p lazily stretches $S wings and gives a single half-hearted flap.",
        "$p tucks $S beak into $S plumage as $E debates a quick nap.",
        "$p makes a few soft, quiet chirps to $Mself as $E relaxes.",
        "$p slowly stretches out $S wings before carefully refolding them."
      }}},
  
  {403, {0, TRUE, "Bird, Playful", "", "", {
        "$p zips around and darts back and forth as $E plays a game of tag with $n.",
        "$p gets the happy feet and loudly tippy taps about.",
        "$p bounces around in place on $S perch as $E watches $n.",
        "$p splashes around a little in $S water dish in amusement.",
        "$p dances around as $E bobs $S head to an unknown rhythm."
      }}},

  {404, {0, TRUE, "Bird, Cuddly", "", "", {
        "$p hops in nice and close to begin preening $n.",
        "$p begins to rub $S head against $n for attention.",
        "$p perches upon $n and fluffs $S feathers up to show off.",
        "$p brings $S favourite toy to show it off to $n.",
        "$p affectionately nuzzles $n with $S beak and head."
      }}},
  
  {405, {0, TRUE, "Bird, Friendly", "", "", {
        "$p hops towards $n in playful pursuit.",
        "$p begins to nonchalantly shadow $n to keep close to $m.",
        "$p bounces and hops around as $E circles $n playfully.",
        "$p flicks and waves $S tail feathers at $n in a friendly gesture.",
        "$p serenades $n with a cute song and dance."
      }}},
  
  {406, {0, TRUE, "Bird, Curious", "", "", {
        "$p tilts $S head as $E intently inspects $n for a while.",
        "$p pecks and taps $S beak on some random object that's caught $S interest.",
        "$p curiously investigates $S reflection in the surface of a random appliance.",
        "$p goes on an unsanctioned adventure through a random container $E found.",
        "$p bobs and flicks $S head as $E curiously investigates something that caught $S eye."
      }}},
  
  {407, {0, TRUE, "Bird, Angry", "", "", {
        "$p stares at $n with the intensity of a thousand suns, silently judging.",
        "$p nibbles a random piece of mail into a bunch of shredded ribbons.",
        "$p stretches $S wings out as $E hops along like an angry little dinosaur.",
        "$p randomly decides that $E needs something more than $n, and $E's willing to fight for it.",
        "$p waits until it's nice and quiet before $E loudly reminds you that $E is, indeed, a bird."
      }}},
    
  {501, {0, TRUE, "Snake, Sleepy", "", "", {
        "$p curls up in a tight bundle of coils as $E snoozes away.",
        "$p sleepily slithers along in search of a cozy niche to nap in.",
        "$p stretches $S body out as $E settles in and relaxes.",
        "$p investigates a nice and cool surface for $E to slumber upon.",
        "$p finds a nice warm spot to enjoy a post-snack nap."
      }}},

  {502, {0, TRUE, "Snake, Lazy", "", "", {
        "$p sprawls out and takes up as much room as possible while $E relaxes.",
        "$p leisurely readjusts $S resting arrangement in an unhurried manner.",
        "$p slowly flicks $S tongue, leaving it partially extended as $E relaxes.",
        "$p half-coils $Mself into a loose bundle to get cozy.",
        "$p occasionally gives a slow and unworried half-flick of $S tongue."
      }}},

  {503, {0, TRUE, "Snake, Playful", "", "", {
        "$p gives a soft, nonaggressive hiss in a playful manner.",
        "$p slithers along in playful patterns and loops.",
        "$p gives a few curled tongue flicks as $E playfully investigates $n.",
        "$p slowly chases after $S own tail, never quite catching it.",
        "$p has officially created a heavily fortified castle out of a pile of old laundry."
      }}},

  {504, {0, TRUE, "Snake, Cuddly", "", "", {
        "$p slithers along past $n and leaves $S tail half-coiled against $m.",
        "$p rapidly flicks $S tongue as $E inspects $n with great interest.",
        "$p gently nuzzles and rubs $S head up against $n for attention.",
        "$p slithers over to $n and curls up nearby to keep $m company.",
        "$p slithers $S way up nice and close to $n for warmth and comfort."
      }}},

  {505, {0, TRUE, "Snake, Friendly", "", "", {
        "$p slithers over and around $n in a clearly unconcerned manner.",
        "$p nuzzles and licks $n with a few little flicks of $S tongue.",
        "$p decides that $n is the most comfortable piece of furniture around here.",
        "$p curiously investigates $n up close in a calm, non-threatening manner.",  
        "$p approaches $n in a slow and calm manner, showing off just how comfortable $E is."
      }}},

  {506, {0, TRUE, "Snake, Curious", "", "", {
        "$p rapidly flicks $S tongue as $E inspects $n with great interest.",
        "$p sets off on an adventure to investigate a newly arrived furnishing.",
        "$p gives a few curled tongue flicks as $E playfully investigates $n.",
        "$p investigates a misplaced piece of clothing someone forgot to put away.",
        "$p slithers along on $S grand adventure, inspecting anything and everything $E can along the way."
      }}},

  {601, {0, TRUE, "Ferret", "", "", {
        "$p excitedly hops around from side to side with an adorable war dance.",
        "$p stalks and pounces upon one of $S toys before rolling around with it.",
        "$p clumsily leaps around and bounces off a piece of furniture.",
        "$p thoroughly investigates and crawls around inside of a piece of clothing that was left unattended.",
        "$p abruptly and swiftly slinks off, leaving behind the sneaking suspicion $E's stolen something."
      }}},

  {602, {0, TRUE, "Hamster", "", "", {
        "$p pops $S cedar-shaving-covered head up and looks around, confused.",
        "$p helps $Mself to the contents of $S sipper water bottle.", 
        "$p bumbles around with $S fluffy cheeks comically filled with who knows what.",
        "$p can be heard going full turbo mode on $S running wheel.",
        "$p busily nibbles on a sunflower seed held in $S front paws."
      }}},

  {603, {0, TRUE, "Fish", "", "", {
        "$p slowly swims back and forth around in $S tank.",
        "$p swiftly darts across the length of $S tank in the blink of an eye.",
        "$p closely inspects a strange little scuba diver and its treasure chest.",
        "$p nibbles on some food that's been sprinkled on the surface of the water.",
        "$p pokes its head out of one the brightly coloured plastic shells in $S tank."
      }}},
  
  {604, {0, TRUE, "Guinea Pig", "", "", {
        "$p lets out a series of excited squeaks, hoping for a treat from $q hands.",
        "$p puffs up and chatters, clearly in a talkative mood.",
        "$p scurries around in quick, excited bursts before freezing as if in deep thought.",
        "$p gently noses at $q fingers, curious and affectionate.",
        "$p releases a happy wheek and begins popcorning around with delight."
      }}},

  {605, {0, TRUE, "Turtle", "", "", {
        "$p slowly blinks at $q presence, seeming to contemplate the mysteries of the universe.",
        "$p stretches $S neck out lazily before retreating back into $S shell with a content sigh.",
        "$p watches $n closely, as if judging $m for some unknown reason.",
        "$p methodically explores the enclosure, every slow step a grand adventure.",
        "$p nibbles at some invisible snack, looking entirely unbothered by the world."      
      }}},

  {606, {0, TRUE, "Chinchilla", "", "", {
        "$p suddenly darts across the room in a blur of fur and energy.",
        "$p flops onto $S side and starts rolling, enjoying a nice, imaginary dust bath.",
        "$p sniffs at $q hand before gently nibbling, testing if it's edible.",
        "$p chitters happily and leaps onto the nearest high surface, defying gravity.",
        "$p wiggles $S cute, little nose and watches $n with big, curious eyes."
      }}},

  {607, {0, TRUE, "Hedgehog", "", "", {
        "$p makes a soft snuffling sound, searching for something interesting to likely nibble on.",
        "$p curls into a tight, spiky ball, only to peek out a moment later to check if the coast is clear. Of what? Who knows.",
        "$p gently bumps $q hand with $S nose before retreating in adorable suspicion.",
        "$p waddles around the room, looking both determined and confused at the same time.",
        "$p lets out a tiny huff before deciding to resume $S noble quest of doing absolutely nothing but be adorable."
      }}},

  {608, {0, TRUE, "Lizard", "", "", {
        "$p flicks $S tongue out, tasting the air around $n.",
        "$p basks under the heat lamp, looking utterly regal and important.",
        "$p tilts $S head at $n, blinking slowly as if pondering deep philosophical questions.",
        "$p climbs onto a nearby artificial rock ledge of $S enclosure, clearly the ruler of this particular domain.",
        "$p freezes mid-step, staring dramatically at something only $E can see."
      }}},
};

void set_up_pet_dummy_mob() {
  dummy_mob_for_pet_pronouns = read_mobile(3, VIRTUAL);

  MOB_FLAGS(dummy_mob_for_pet_pronouns).SetBits(MOB_ISNPC, MOB_INANIMATE, MOB_NOKILL, MOB_SENTINEL, ENDBIT);

  char_to_room(dummy_mob_for_pet_pronouns, &world[1]);

  // Remove it from the mobile list, it will never act.
  for (struct char_data *ch = character_list, *prev_ch = NULL; ch; ch = ch->next_in_character_list) {
    if (ch == dummy_mob_for_pet_pronouns) {
      if (prev_ch == NULL) {
        character_list = ch->next_in_character_list;
      } else {
        prev_ch->next_in_character_list = ch->next_in_character_list;
        ch->next_in_character_list = NULL;
      }
      break;
    }
  }
}

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

void pet_acts(struct obj_data *pet, int pet_act_filter) {
  if (!pet->in_room || !pet->in_room->people || !pet->in_room->apartment)
    return;

#ifndef IS_BUILDPORT
  if ((GET_OBJ_IDNUM(pet) % 10) != pet_act_filter)
    return;
#endif

  class PetEchoSet *selected_echo_set = get_pet_echo_set(GET_PET_ECHO_SET_IDNUM(pet));
  GET_PRONOUNS(dummy_mob_for_pet_pronouns) = GET_PET_PRONOUN_SET(pet);

  if (!selected_echo_set) {
    mudlog_vfprintf(NULL, LOG_SYSLOG, "SYSERR: Hit pet_acts(%s) with pet that had invalid echo_set %d.", GET_PET_ECHO_SET_IDNUM(pet));
    return;
  }

  act(selected_echo_set->get_random_echo_message(), FALSE, pet->in_room->people, pet, dummy_mob_for_pet_pronouns, TO_NOTVICT);
}

void create_pet_main_menu(struct descriptor_data *d) {
  class PetEchoSet *selected_echo_set = get_pet_echo_set(GET_PET_ECHO_SET_IDNUM(PET));
  GET_PRONOUNS(dummy_mob_for_pet_pronouns) = GET_PET_PRONOUN_SET(PET);

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
      act(message, FALSE, CH, PET, dummy_mob_for_pet_pronouns, TO_CHAR | TO_SLEEP);
    }
  }
  send_to_char(CH, "5) ^cPronouns: ^n%s/%s^n\r\n", HSSH(dummy_mob_for_pet_pronouns), HMHR(dummy_mob_for_pet_pronouns));
  send_to_char(CH, "6) ^cWanders:  ^n%s\r\n",
               GET_PET_WANDER_MODE(PET) == PET_WANDER_MODE_NOTSET ? "<not set>" : 
                 (GET_PET_WANDER_MODE(PET) == PET_WANDER_MODE_WANDERS ? "Yes, between apartment rooms" : "No, stays in the room you drop it in"));
  send_to_char("\r\n", CH);
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
        case '5':
          {
            for (int idx = 0; idx < NUM_PRONOUNS; idx++) {
              GET_PRONOUNS(dummy_mob_for_pet_pronouns) = idx;
              send_to_char(CH, "^c%d^n) %s/%s\r\n", idx, HSSH(dummy_mob_for_pet_pronouns), HMHR(dummy_mob_for_pet_pronouns));
            }
            send_to_char(CH, "\r\nSelect the pronouns you'd like to see in your pet's flavor messages: ");
            d->edit_mode = PET_EDIT_PRONOUNS;
          }
          break;
        case '6':
          {
#ifdef PETS_MOVE_AROUND
            send_to_char(CH, "Enter 1 if this pet can roam around in apartments, or 2 if it should stay where you set it down: ");
#else
            send_to_char(CH, "Pet wandering will be implemented in a future patch, but please set this value correctly. Enter 1 if this pet can roam around in apartments, or 2 if it should stay where you set it down: ");
#endif
            d->edit_mode = PET_EDIT_WANDER;
          }
        case 'q':
        case 'Q':
          {
            if (!PET->graffiti
                || !PET->restring
                || !str_cmp(GET_OBJ_NAME(PET), "a custom pet")
                || GET_PET_ECHO_SET_IDNUM(PET) == 0
                || GET_PET_WANDER_MODE(PET) == PET_WANDER_MODE_NOTSET)
            {
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
        default:
          send_to_char("That's not a valid option. Pick a number from 1-5, or enter Q to save or X to discard changes: ", CH);
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

        // Messages sent in function.
        if (check_for_banned_content(arg, CH))
          return;

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
    case PET_EDIT_PRONOUNS:
      {
        int number = atoi(arg);
        if (number < 0 || number >= NUM_PRONOUNS) {
          send_to_char(CH, "Sorry, %d isn't a valid option. Try again: ", number);
          return;
        }

        GET_PET_PRONOUN_SET(PET) = number;
        create_pet_main_menu(d);
        return;
      }
      break;
    case PET_EDIT_WANDER:
      {
        int number = atoi(arg);
        if (number < 1 || number > 2) {
          send_to_char(CH, "Sorry, %d isn't a valid option. Try again: ", number);
          return;
        }

        GET_PET_WANDER_MODE(PET) = number;
        create_pet_main_menu(d);
        return;
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

  send_to_char(ch, "You are entering pet creation, which allows you to create custom pets that live in your apartment.\r\n"
                   "Please note that these ^Wpets are for RP flavor only^n; they do not provide any mechanical benefits.\r\n"
                   "Please also remember that ^Wpets should be animals^n (not people, robots, spirits, etc).\r\n\r\n");

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