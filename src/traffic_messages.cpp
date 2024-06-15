#include <vector>
#include <algorithm>
#include <random>

#include "structs.hpp"
#include "db.hpp"

#define TRAFFIC_MESSAGES_NORMAL 0
#define TRAFFIC_MESSAGES_DEPRESSING 1
#define NUM_TRAFFIC_MESSAGE_TYPES 2

std::vector<const char *> random_message_queue[NUM_TRAFFIC_MESSAGE_TYPES];
const char *current_traffic_msg[NUM_TRAFFIC_MESSAGE_TYPES] = {0};

void regenerate_traffic_msgs();

void initialize_traffic_msgs() {
  for (int which = 0; which < NUM_TRAFFIC_MESSAGE_TYPES; which++) {
    random_message_queue[which].clear();
  }

  regenerate_traffic_msgs();
}

const char *traffic_messages[] = {
  "A man on a Yamaha Rapier zips by.\r\n", // 0
  "A Mitsuhama Nightsky limousine slowly drives by.\r\n",
  "A Ford Bison drives through here, splashing mud on you.\r\n",
  "A Lone Star squad car drives by, sirens blaring loudly.\r\n",
  "An orkish woman drives through here on her Harley Scorpion.\r\n",
  "An elf drives through here on his decked-out Yamaha Rapier.\r\n", // 5
  "A ^rred^n Chrysler-Nissan Jackrabbit cruises by.\r\n",
  "A ^yyellow^n Chrysler-Nissan Jackrabbit cruises by.\r\n",
  "A ^Wwhite^n Chrysler-Nissan Jackrabbit cruises by.\r\n",
  "A ^rred^n Ford Americar cruises by.\r\n",
  "A ^yyellow^n Ford Americar cruises by.\r\n", // 10
  "A ^Wwhite^n Ford Americar cruises by.\r\n",
  "A ^Bblue^n Ford Americar cruises by.\r\n",
  "A ^Bblue^n Chrysler-Nissan Jackrabbit cruises by.\r\n",
  "A ^Rcherry red^n Eurocar Westwind 2000 flies past you.\r\n",
  "A ^Wwhite^n Mitsubishi Runabout drives by slowly.\r\n", // 15
  "A ^bblue^n Mitsuhama Runabout drives by slowly.\r\n",
  "An elven woman on a Dodge Scoot passes through here.\r\n",
  "A ^Ybright yellow^n Volkswagen Electra passes by silently.\r\n",
  "A huge troll rides by on a modified BMW Blitzen 2050.\r\n",
  "A large, ^Wwhite^n GMC Bulldog van drives through here.\r\n", // 20
  "A DocWagon ambulance speeds past, its lights flashing brightly.\r\n",
  "The deep thrum of a helicopter passes swiftly overhead.\r\n",
  "A rugged-looking dwarf on a Rhiati Razor howls past.\r\n",
  "A MTC-Nissan roto-drone floats quietly on by.\r\n",
  "A souped-up Saab Dynamit 778 TI purrs past you.\r\n", // 25
  "A bleary-eyed wage slave putters past on an underpowered moped.\r\n",
  "An overloaded GMC Bulldog Security with open gun ports rumbles past.\r\n",
  "The sound of squealing tires echoes from somewhere in the distance.\r\n",
  "A troll on a rusted bicycle pedals squeakily by.\r\n",
  "A badly doppler-shifted track from The Elementals follows a truck speeding by.\r\n", // 30
  "A ^Lmatte-black^n LAV-93 roars through, narrowly missing you.\r\n",
  "A few sporadic pops of gunfire sound from somewhere in the distance.\r\n",
  "The stench of garbage wafts from somewhere nearby.\r\n",
  "A harried-looking salaryman hurries by.\r\n",
  "A backfiring Ford-Canada Bison splutters past.\r\n", // 35
  "A sleek ^rred^n roto-drone zips past.\r\n",
  "A crumpled-up plastic bag skitters past, carried by the wind.\r\n",
  "A poised and confident executive strides past, talking on her phone.\r\n",
  "A subdued-looking teen on a scooter whizzes by on his way to class.\r\n",
  "A billboard nearby flickers with an ad for ^RChernobyl Vodka^n.\r\n", // 40
  "A billboard nearby displays an ad for ^rBrimstone ^RRed^n Ale^n.\r\n",
  "The greasy scent of fast food is carried to you on the breeze.\r\n"
};
#define NUM_TRAFFIC_MESSAGES 43

const char *depressing_traffic_messages[] = {
  "A crumpled plastic bag slips past on an eddy of wind.\r\n",  // 0
  "A long string of automatic fire echoes off of the concrete jungle.\r\n",
  "A high-pitched screech warbles somewhere in the distance.\r\n",
  "A flickering shadow zips above you, too high up to make out any details.\r\n",
  "The sound of squealing tires echoes from somewhere in the distance.\r\n",
  "The moldering scent of rot wafts by on the breeze.\r\n", // 5
  "The stench of rotten eggs floats up from the river.\r\n",
  "A fishy smell sneaks its way in from somewhere.\r\n",
  "There's an ominous mutter from somewhere nearby, but it fades too fast for you to trace it.\r\n",
  "A quiet chittering filters up from somewhere beneath you.\r\n",
  "The skitter of too many legs sounds from somewhere nearby.\r\n", // 10
  "A crumbling bit of masonry clatters to the ground.\r\n",
  "Dust sifts down from somewhere overhead.\r\n",
  "The groan of straining metal echoes through the streets.\r\n",
  "The distant wail of an alarm rises and falls, unanswered.\r\n",
  "A broken window pane creaks as it sways in the wind.\r\n", // 15
  "Leaves rustle across the cracked pavement, driven by sporadic gusts.\r\n",
  "The faint smell of burnt rubber hangs in the air.\r\n",
  "A rusty signpost squeaks as it swings.\r\n",
  "A muted rumble hints at something collapsing far away.\r\n",
  "A brief, bright flash of light illuminates the horizon.\r\n", // 20
  "A metallic clinking sound echoes, like a loose chain swaying in the breeze.\r\n",
  "The persistent drip of water echoes through a nearby alley.\r\n",
  "A stray screamsheet flutters by, its pages torn and yellowed.\r\n",
  "A single light bulb flickers erratically in an otherwise darkened window.\r\n",
  "A plume of black smoke rises in the distance.\r\n", // 25
  "A lone bird circles high above, its cries lost in the clouded sky.\r\n",
  "An old billboard, faded and peeling, advertises long-forgotten luxuries.\r\n",
  "The acrid smell of smoke lingers in the air, a remnant of recent fires.\r\n",
  "The pungent odor of old gasoline seeps from somewhere nearby.\r\n",
  "A whiff of charred wood drifts by on the breeze.\r\n", // 30
  "The scent of mildew and dampness wafts from an abandoned subway entrance.\r\n",
  "The sharp tang of rust suddenly permeates the air.\r\n",
  "The stench of spoiled food assaults your nostrils.\r\n",
  "The faint, sweet smell of decomposing vegetation rises from a neglected bit of green space.\r\n",
  "The heavy, oily scent of stagnant water flows past you.\r\n", // 35
  "The clang of metal on metal echoes from an unseen alley.\r\n",
  "A hollow rattle comes from an empty soda can rolling down the street.\r\n",
  "A flight of pigeons startles from a nearby rooftop.\r\n",
  "There's an eerie howl as the wind whips through shattered windows.\r\n",
  "The sudden crash of a collapsing fire escape reverberates through the silence.\r\n", // 40
  "The clatter of falling debris echoes down the empty streets.\r\n",
  "A muffled explosion booms from somewhere in the city.\r\n",
  "The shrill cry of a seagull pierces the silence.\r\n",
  "The distant bark of a stray dog reverberates amidst the concrete jungle.\r\n",
  "You become aware of a rustling noise-- probably rats scurrying through the garbage.\r\n", // 45
  "There's a sharp crack as glass breaks somewhere nearby.\r\n",
  "From a high-up window, you hear the mechanical murmur of an emergency broadcast repeating endlessly.\r\n",
  "The rotten smell of stagnant water blows through on a cold breeze.\r\n",
  "A sudden flurry of ash swirls through the air, settling like snow on the abandoned street.\r\n",
  "The muffled thump of something heavy falling nearby sends a tremor through the ground.\r\n", // 50
  "A ragged piece of paper flutters down from above, its edges browned and brittle.\r\n",
  "A flickering neon sign buzzes weakly, casting erratic shadows across the pavement.\r\n",
  "A tangled mess of wires dangles from a broken streetlamp, swaying gently.\r\n",
  "You spot movement as a large rat drags a chunk of carcass out of sight.\r\n",
  "A faint, acidic smell lingers in the air, hinting at something chemical.\r\n", // 55
  "A string of broken fairy lights hangs from a balcony, some bulbs still flickering sporadically.\r\n",
  "The dull thud of something hitting the water echoes from the lake's edge.\r\n"
};
#define NUM_DEPRESSING_MESSAGES 57

void regenerate_traffic_msgs() {
  for (int which = 0; which < NUM_TRAFFIC_MESSAGE_TYPES; which++) {
    auto queue = random_message_queue[which];
    const char **messages = traffic_messages;

    switch (which) {
      case TRAFFIC_MESSAGES_DEPRESSING:
        messages = depressing_traffic_messages;
        break;
      default:
        mudlog_vfprintf(NULL, LOG_SYSLOG, "SYSERR: Got unknown traffic message type to regenerate_traffic_msg_queue(%d). Using standard traffic.", which);
        // fall through
      case TRAFFIC_MESSAGES_NORMAL:
        // No change, standard traffic is the default for messages.
        break;
    }

    // Regenerate the queue if needed.
    if (queue.empty()) {
      // Add traffic messages to the queue.
      for (int msg_idx = 0; msg_idx < (which == 0 ? NUM_TRAFFIC_MESSAGES : NUM_DEPRESSING_MESSAGES); msg_idx++)
        queue.push_back(messages[msg_idx]);

      // Shuffle it.
      std::random_shuffle(queue.begin(), queue.end());
    }

    // Assign this loop's traffic message.
    current_traffic_msg[which] = queue.back();
    queue.pop_back();
  }
}

SPECIAL(traffic)
{
  struct room_data *room = (struct room_data *) me;

  if (!cmd && room->people) {
    if (GET_JURISDICTION(room) == JURISDICTION_SECRET) {
      send_to_room(current_traffic_msg[TRAFFIC_MESSAGES_DEPRESSING], room);
    } else {
      send_to_room(current_traffic_msg[TRAFFIC_MESSAGES_NORMAL], room);
    }
  }

  return FALSE;
}