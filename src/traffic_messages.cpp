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

const char *_vehicle_adjectives[] = {
  "oversized",
  "shiny",
  "banged-up",
  "slightly-on-fire",
  "pristine",
  "lifted",
  "modded",
  "sticker-covered",
  "graffitied",
  "stolen",
  "rusted-out",
  "clapped-out",
  "annoyingly loud",
  "candy-painted",
  "chromed-out",
  "armored",
  "bullet-hole riddled",
  "sticky-looking",
  "greasy",
  "decked-out"
};
#define _NUM_VEHICLE_ADJECTIVES 20

const char *_vehicle_colors[] = {
  "^Wbright white",
  "^Wwhite",
  "^Wwhite",
  "^Wwhite",
  "^wgrey",
  "^Lmatte-black",
  "^Ldeep black",
  "^Lblack",
  "^bdark blue",
  "^Bblue",
  "^gdark green",
  "^gforest green",
  "^Ggreen",
  "^mpurple",
  "^Mmagenta",
  "^rred",
  "^rred",
  "^Rcherry red",
  "^yyellow",
  "^Ybright yellow",
  "^Rr^Oa^Yi^Gn^Bb^[F225]o^Vw^n-painted"
};
#define _NUM_VEHICLE_COLORS 21

const char *_vehicle_types__ride_on[] = {
  "Dodge Scoot", // 0
  "Dodge Scoot",
  "Dodge Scoot",
  "Honda Viking",
  "Honda Viking",
  "Suzuki Aurora",
  "Suzuki Aurora",
  "Yamaha Katana",
  "Yamaha Katana",
  "BMW Blitzen",
  "BMW Blitzen", // 10
  "Rhiati Kender scooter",
  "Rhiati Kender scooter",
  "E.S. Papoose scooter",
  "E.S. Papoose scooter",
  "Yamaha Rapier",
  "Rhiati Arrow",
  "Rhiati Razor",
  "racing bike",
  "moped",
  "moped", // 20
  "road hog",
  "crotch rocket",
  "gas-powered skateboard",
  "delivery bike",
  "motorized unicycle",
  "escaped roomba", // 26
};
#define _NUM_VEHICLE_TYPES__RIDE_ON 27

const char *_vehicle_types__ride_in[] = {
  "Ford Americar",
  "Ford Americar",
  "Ford Americar",
  "Jackrabbit",
  "Jackrabbit",
  "Eurocar Westwind",
  "Eurocar Westwind",
  "Ford-Canada Bison",
  "Ford-Canada Bison",
  "GMC Bulldog",
  "GMC Bulldog",
  "Toyota Gopher pickup",
  "Toyota Gopher pickup",
  "taxi cab",
  "taxi cab",
  "Volkswagen Elektro",
  "Rolls-Royce Prairie Cat",
  "Ford Mustang",
  "Rolls-Royce Phaeton limousine",
  "Toyota Elite",
  "Land Rover Model 2046",
  "Ares RoadMaster",
  "Leyland-Rover Transport pickup",
  "Leyland-Rover Transport minibus",
  "Rhiati Destion limousine",
  "GMC Bulldog Security",
  "Impala",
  "Sikorsky-Bell Red Ranger hovercraft",
  "GMC-Nissan hovertruck",
  "Gaz-Willys Nomad SUV",
  "Nissan-Holden Brumby SUV",
  "Honda-GM 3220 sports car",
  "Leyland-Zil Tsarina",
  "Conestoga Trailblazer",
  "ambulance",
  "fire truck",
  "APC"
};
#define _NUM_VEHICLE_TYPES__RIDE_IN 37

const char *_vehicle_speeds[] = {
  "putters by", // 0
  "splutters on through",
  "backfires as it passes",
  "idles past",
  "speeds past",
  "speeds past you",
  "races past",
  "races past you",
  "drives by",
  "drives by you",
  "drives by slowly", // 10
  "slowly drives by",
  "limps through",
  "tears past",
  "screeches by",
  "absolutely howls through",
  "flies past you" // 16
};
#define _NUM_VEHICLE_SPEEDS 17

const char *_vehicle_addendums[] = {
  "splashing mud on you",
  "splashing mud on you",
  "splashing mud on you",
  "splashing mud on you",
  "splashing mud on you",
  "splashing mud on you",
  "narrowly missing you",
  "narrowly missing you",
  "narrowly missing you",
  "narrowly missing you",
  "narrowly missing you",
  "blasting music",
  "blasting music",
  "blasting music",
  "overloaded with way too many passengers",
  "sounding like it's on its last gasp",
  "followed closely by the sound of gunfire",
  "chased by a puffing Lone Star officer",
  "trailing debris",
  "dripping with something truly awful",
  "leaving a heavy stench of weed in the air",
  "dangling an unsecured load",
  "causing problems",
};
#define _NUM_VEHICLE_ADDENDUMS 23

const char *_person_adjectives[] = {
  "hulking",
  "roided-up",
  "diminutive",
  "petite",
  "broad-shouldered",
  "slope-shouldered",
  "beetle-browed",
  "lanky",
  "emaciated",
  "rotund",
  "one-eyed",
  "beat-up",
  "bearded",
  "long-haired",
  "heavily perfumed",
  "rank",
  "fragrant",
  "tiny",
  "average",
  "huge",
  "placid-looking",
  "muttering",
  "scowling",
  "maniacally grinning",
  "inebriated",
  "hopped-up",
  "haunted-looking",
  "agitated",
  "grinning",
  "lab-coat-wearing",
  "leather-clad",
  "well-dressed",
  "slovenly",
  "cheery",
  "rugged-looking",
  "bleary-eyed"
};
#define _NUM_PERSON_ADJECTIVES 36

const char *_person_shortdescs[] = {
  "troll",
  "elf",
  "dwarf",
  "man",
  "woman",
  "enby",
  "blond",
  "blonde",
  "brunette",
  "dude",
  "person",
  "merc",
  "wage slave",
  "rocker",
  "techie",
  "ganger",
  "drifter",
  "wastrel",
  "'runner",
  "figure that's definitely not several raccoons stacked on top of each other in a trench coat",
};
#define _NUM_PERSON_SHORTDESCS 20

const char *generate_dynamic_traffic_message__returns_new() {
  char msg_buf[1000] = { '\0' };
  char out_buf[1000];
  bool on_vs_in = !number(0, 3); //25% chance of "on"

  switch (number(0, 0)) {
    case 0:
      // "<A/An><maybe adjective> <tag><color>^n <vehicle> <speed>> by<maybe addendum, like splashing mud on you>.\r\n",
      // Add a random adjective 25% of the time.
      if (!number(0, 3)) {
        snprintf(ENDOF(msg_buf), sizeof(msg_buf) - strlen(msg_buf), "%s^n ", _vehicle_adjectives[number(0, _NUM_VEHICLE_ADJECTIVES - 1)]);
      }

      // Add a color 40% of the time.
      if (number(0, 9) < 4) {
        snprintf(ENDOF(msg_buf), sizeof(msg_buf) - strlen(msg_buf), "%s^n ", _vehicle_colors[number(0, _NUM_VEHICLE_COLORS - 1)]);
      }

      // Add a vehicle type.
      {
        const char *selected_vehicle;

        if (on_vs_in) {
          selected_vehicle = _vehicle_types__ride_on[number(0, _NUM_VEHICLE_TYPES__RIDE_ON - 1)];
        } else {
          selected_vehicle = _vehicle_types__ride_in[number(0, _NUM_VEHICLE_TYPES__RIDE_IN - 1)];
        }

        snprintf(ENDOF(msg_buf), sizeof(msg_buf) - strlen(msg_buf), "%s^n ", selected_vehicle);
      }
      
      // Add a speed.
      snprintf(ENDOF(msg_buf), sizeof(msg_buf) - strlen(msg_buf), "%s^n", _vehicle_speeds[number(0, _NUM_VEHICLE_SPEEDS - 1)]);

      // Add a random addendum 10% of the time.
      if (!number(0, 9)) {
        snprintf(ENDOF(msg_buf), sizeof(msg_buf) - strlen(msg_buf), ", %s^n", _vehicle_addendums[number(0, _NUM_VEHICLE_ADDENDUMS - 1)]);
      }
      break;
  }

  // Sometimes, add a person as the occupant. Always for ridden vehicles, sometimes for occupied ones.
  if (on_vs_in || !number(0, 4)) {
    char person_buf[1000] = { '\0' };

    if (number(0, 3)) {
      snprintf(ENDOF(person_buf), sizeof(person_buf) - strlen(person_buf), "%s^n ", _person_adjectives[number(0, _NUM_PERSON_ADJECTIVES - 1)]);
    }
    snprintf(ENDOF(person_buf), sizeof(person_buf) - strlen(person_buf), "%s^n", _person_shortdescs[number(0, _NUM_PERSON_SHORTDESCS - 1)]);

    // stitch it togther
    snprintf(out_buf, sizeof(out_buf), "%s %s %s %s %s.\r\n", CAP(AN(person_buf)), person_buf, on_vs_in ? "on" : "in", AN(msg_buf), msg_buf);
  } else {
    // Otherwise, just prepend A/An.
    snprintf(out_buf, sizeof(out_buf), "%s %s.\r\n", CAP(AN(msg_buf)), msg_buf);
  }

  // Finally, return it as a str_dup string.
  return str_dup(out_buf);
}

const char *traffic_messages[] = {
  "A Lone Star squad car drives by, sirens blaring loudly.\r\n", // 0
  "A DocWagon ambulance speeds past, its lights flashing brightly.\r\n",
  "The deep thrum of a helicopter passes swiftly overhead.\r\n",
  "A MTC-Nissan roto-drone floats quietly on by.\r\n",
  "An overloaded GMC Bulldog Security with open gun ports rumbles past.\r\n",
  "The sound of squealing tires echoes from somewhere in the distance.\r\n", // 5
  "A troll on a rusted bicycle pedals squeakily by.\r\n",
  "A badly doppler-shifted track from The Elementals follows a truck speeding by.\r\n",
  "A ^Lmatte-black^n LAV-93 roars through, narrowly missing you.\r\n",
  "A few sporadic pops of gunfire sound from somewhere in the distance.\r\n",
  "The stench of garbage wafts from somewhere nearby.\r\n", // 10
  "A harried-looking salaryman hurries by.\r\n",
  "A backfiring Ford-Canada Bison splutters past.\r\n",
  "A sleek ^rred^n roto-drone zips past.\r\n",
  "A crumpled-up plastic bag skitters past, carried by the wind.\r\n",
  "A poised and confident executive strides past, talking on her phone.\r\n", // 15
  "A subdued-looking teen on a scooter whizzes by on his way to class.\r\n",
  "A billboard nearby flickers with an ad for ^RChornobyl Vodka^n.\r\n",
  "A billboard nearby displays an ad for ^rBrimstone ^RRed^n Ale^n.\r\n",
  "The greasy scent of fast food is carried to you on the breeze.\r\n",
  "A drone hums low overhead, its red lights scanning the dark streets for any sign of trouble.\r\n", // 20
  "The aroma of charred synthmeat lingers from a nearby street vendor.\r\n",
  "A group of high-end mercs with heavy chrome stalk by, scanning every corner.\r\n",
  "A garbage truck rumbles by, its hydraulics squealing as it collects the day's refuse, leaving a trail of oil and plastic scraps behind.\r\n",
  "A muffled explosion in the distance makes the ground vibrate.\r\n",
  "A street sweeper hums lazily by, its brush scraping against the gritty pavement, stirring up dust and forgotten debris.\r\n", // 25
  "A steady drip of water echoes from a broken pipe overhead, its rhythm syncopated by the occasional hiss of steam from an overheated vent.\r\n",
  "The scent of stale booze lingers in the air as a group of drunks stumbles past.\r\n",
  "The sharp scent of ozone stings your nostrils.\r\n",
  "A trace of someone's perfume drifts past, synthetic and cloying.\r\n",
  "The acrid tang of burnt garbage clings to the cracked pavement.\r\n", // 30
  "Sizzling meat and frying oil fill the air from somewhere nearby, greasy and inviting.\r\n",
  "The stench of burnt rubber lingers in the air.\r\n",
  "A gnarly mix of damp leather, cigarette smoke, and stale whiskey wafts in on the breeze.\r\n",
  "A sharp, antiseptic scent is carried by on the wind.\r\n",
  "A crackling speaker blares a distorted ad in the distance.\r\n", // 35
  "A siren wails in the distance, its pitch rising and falling slowly.\r\n",
  "The hiss of steam escapes from a broken valve nearby.\r\n",
  "The soft buzz of a hovering drone is barely audible overhead.\r\n",
  "A dog's frantic barking echoes off the walls of the narrow street.\r\n",
  "A dwarf kid on a bicycle zooms past, nearly knocking you over.\r\n", // 40
  "A gaggle of rowdy teenagers shout and shove each other as they make their way down the street.\r\n",
  "A garbage truck crawls its way down the street with an occasional thud and clatter as bags are thrown in.\r\n",
  "A rat sticks its nose out of a drain, sniffs the air a moment, then decides it smells better in the sewers.\r\n",
  "You see the movement of a tiny shadow as a mangy rat crosses a nearby alleyway.\r\n",
  "You hear the roar of a souped-up engine in the distance.\r\n", // 45
  "You hear the distant sound of someone vomiting in an alleyway.\r\n",
  "A company man walks by and flicks a toothpick at you.\r\n",
  "A flock of pigeons startles into the air from a nearby power line.\r\n",
  "A seagull swoops down and steals a bit of food from a passerby.\r\n",
  "A swarm of flies bumps into you, buzzing in your ears.\r\n", // 50
  "A stray cat darts across your field of vision.\r\n",
  "The sound of a couple arguing comes from a nearby window.\r\n",
  "A firetruck blazes through with sirens blaring, heading to some unknown destination.\r\n",
  "An overworked garbage truck comes through collecting refuse.\r\n",
  "A group of gangers drive by slowly in a vintage muscle car.\r\n", // 55
  "Two opposing motorcycle gangs fly by at breakneck speed, their battle sending stray rounds cracking off the pavement.\r\n",
  "A beat-up junker of a car putt-putts by, its muffler scraping the ground.\r\n",
  "You catch the vague movement of a shadowy figure leaping between rooftops, but they're gone when you take a closer look.\r\n",
  "A city bus drives by with a billboard on the side advertising some local politican.\r\n",
  "An ad for the latest Novatech cyberdeck flashes on a billboard nearby.\r\n", // 60
  "An advertisement hologram depicting a nuclear fission power station comes to life nearby, followed by the sentence \"Shiawase: Advancing Life\".\r\n",
  "A cheap neon light illuminates graffiti over an old poster depicting Renraku's latest Plant Research.\r\n",
  "A taxi drives by with a billboard on top depicting Saeder-Krupp's latest aerospace endeavours.\r\n",
  "A bus drives by with a side poster showcasing Maria Mercurial's upcoming concert.\r\n",
  "A group of Mitsuhama executives walk by, their bodyguards shoving people aside.\r\n", // 65
  "A crouched junkie drools as he jacks into the latest BTL, right underneath a poster advertising Yamatetsu's latest bioware advancements.\r\n",
  "An electronic billboard flickers to life with the message \"Kitsune in concert, presented by Manasonic!\"\r\n",
  "A girl on a skateboard hooks a grapple onto a passing truck and zips away.\r\n",
  "A drunk vomits noisily into the gutter somewhere nearby.\r\n",
  "A chromed-up samurai strides past, acting like he's hot drek.\r\n", // 70
  "A trio of Knight Errant security vehicles cut through the streets like sharks.\r\n",
  "A street vendor wanders past, loudly hawking his authentic Meat-In-A-Bun.\r\n",
  "A dead-eyed wage slave stares blankly at nothing for a long time, then wanders off.\r\n",
  "A litter of ork children run past in a screaming gaggle.\r\n",
  "A strange smell wafts out of a dark alley, reminding you of... No, it's nothing.\r\n", // 75
  "There's a loud CRASH as two vehicles are involved in a minor accident. The shouting and gesticulating go on for quite a while.\r\n",
  "A stray dog pisses on a fire hydrant, then wanders off happily.\r\n",
  "A street magician does a few parlor tricks on the corner before heading off. He was pretty good.\r\n" // 78
};
#define NUM_TRAFFIC_MESSAGES 79

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
  "A rusty signpost squeaks in the breeze.\r\n",
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
  "The muffled thud of something heavy falling nearby sends a tremor through the ground.\r\n", // 50
  "A ragged piece of paper flutters past, its edges browned and brittle.\r\n",
  "A flickering neon sign buzzes weakly, casting erratic blots of color across the pavement.\r\n",
  "A tangled mess of wires dangles from a broken streetlamp, swaying gently.\r\n",
  "You spot movement as a large rat drags a chunk of carcass out of sight.\r\n",
  "A faint, acidic smell lingers in the air, hinting at something chemical.\r\n", // 55
  "A string of broken fairy lights hangs from a balcony, some bulbs still flickering sporadically.\r\n",
  "The dull thud of something hitting the water echoes from the lake's edge.\r\n",
  "The sharp crack of high-powered rifle fire echoes from somewhere in the distance.\r\n",
  "A shrill scream rises up before abruptly being silenced.\r\n",
  "The sickly-sweet smell of decomposing flesh assaults your nostrils.\r\n", // 60
  "The sound of a collapsing billboard echoes through the streets, the squeal of failing metal cutting through the air.\r\n",
  "The rhythmic thump-thump-thump of a helicopter passes far overhead.\r\n",
  "A low, mournful tone, like a distant foghorn, sounds from the direction of the water.\r\n",
  "A cascade of glass tinkles down to the pavement as a window frame finally gives way.\r\n",
  "The faint, discordant notes of a piano drift down from a ruined apartment building.\r\n", // 65
  "A low hum emanates from a nearby power transformer before sputtering into silence.\r\n",
  "The gurgle of liquids moving through a broken sewer pipe briefly occupies your hearing.\r\n",
  "A loose manhole cover rattles as something moves in the tunnels below.\r\n",
  "The unexpected roar of an engine echoes through the streets, loud and close, before quickly receding.\r\n",
  "A swarm of flies buzzes loudly over something unseen in a darkened alley.\r\n", // 70
  "The high-pitched whine of stressed metal comes from a damaged bridge in the distance.\r\n",
  "A child's laughter, tinny and distorted, echoes from a broken toy somewhere nearby.\r\n",
  "The crunch of gravel under a heavy boot sounds from just around the corner, then fades.\r\n",
  "A series of short, sharp barks echoes from a nearby tenement.\r\n",
  "The ground trembles faintly, a deep vibration coming from far below the surface.\r\n", // 75
  "A curtain flutters from a shattered window high above you.\r\n",
  "Steam vents from a manhole cover, hissing into the cold air.\r\n",
  "A flock of crows takes to the sky, cawing loudly as they scatter from a nearby roof.\r\n",
  "For a moment, a powerful searchlight sweeps across the sky from the city's quarantine wall.\r\n",
  "A puddle of iridescent, oily fluid shimmers on the asphalt.\r\n", // 80
  "Light glints off something metallic from a rooftop several blocks away.\r\n",
  "A pack of gaunt, wild dogs trots across a distant intersection, vanishing into the ruins.\r\n",
  "A lone shopping cart rolls slowly across the street, pushed by the wind.\r\n",
  "The air suddenly thickens with the coppery smell of old blood.\r\n",
  "A sharp, sterile scent, like antiseptic, drifts out of a shattered window.\r\n", // 85
  "A wave of ozone, sharp and electric, washes over you after a distant flash.\r\n",
  "An overpowering floral scent, incongruous in the decay, drifts from a rooftop garden gone wild.\r\n",
  "A thick, yeasty smell, like a brewery left to rot, hangs heavy in the air.\r\n",
  "The air smells of salt and drying seaweed from the nearby coast.\r\n",
  "A bitter, almond-like chemical smell catches in the back of your throat for a moment.\r\n", // 90
  "The wind picks up, carrying a fine grit that stings your eyes.\r\n",
  "The grating sound of a heavy metal panel being dragged somewhere nearby stops as abruptly as it began.\r\n",
  "A single, clear bell tone rings out and is then suddenly muffled.\r\n",
  "The frantic flapping of wings erupts from just inside a darkened doorway.\r\n",
  "A sharp hiss, like a punctured tank releasing pressure, sounds from a wrecked vehicle.\r\n", // 95
  "An automated voice from a derelict public bus sputters a fragment of a route number before dying.\r\n",
  "A car alarm begins to blare, only to short out with a pathetic, descending squeal.\r\n",
  "A single, dry cough echoes from a floor above you, followed by silence.\r\n",
  "A reflection of rapid movement flashes across a shard of glass, but when you look, nothing is there.\r\n",
  "A huge sheet of plastic, torn from some forgotten advertisement, unfurls in the wind like a sail before collapsing again.\r\n", // 100
  "A stream of rusty water suddenly gushes from a high downspout, staining the wall, and then trickles to a stop.\r\n",
  "A vacant window flickers with light several blocks away, but it's immediately extinguished.\r\n",
  "A vortex of leaves and street garbage swirls up into the air, held there for a moment before scattering.\r\n",
  "A chunk of concrete falls from a high ledge, shattering on the sidewalk with a loud crack.\r\n",
  "You hear the hollow, repetitive bang of a windblown door slamming somewhere above you.\r\n", // 105
  "A loose piece of sheet metal on a roof flips over in the wind, flashing a brief, brilliant burst of reflected light.\r\n",
  "The sharp, acrid smell of burning plastic assaults your nostrils and is just as quickly gone.\r\n",
  "A sudden, thick wave of rotgut alcohol, sharp and cheap, washes over you.\r\n",
  "For a moment, the air is thick with the smell of leaking natural gas.\r\n",
  "A foul, sulfurous stench bubbles up from a sewer grate and then dissipates into the wind.\r\n", // 110
  "The unsettlingly savory scent of burning meat wafts from somewhere nearby before vanishing.\r\n",
  "The pungent, earthy smell of freshly disturbed mold rises from a nearby pile of rubble.\r\n",
  "A phantom smell, like old, wet books, hangs in the air for a moment before vanishing.\r\n",
  "The air briefly carries the heavy, metallic tang of rust and decay.\r\n",
  "The ground shudders as a deep, resonant boom echoes from across the city.\r\n", // 115
  "A sudden, cold downdraft pours out of a narrow alley, raising goosebumps on your skin.\r\n",
  "A fine, greasy mist settles on your exposed skin for a moment before the wind carries it away.\r\n",
  "A metal can, propelled by a gust of wind, clatters loudly down the street before coming to a rest in the gutter.\r\n",
  "A sheet of metal siding tears loose from a rooftop and spins to the ground with a deafening shriek and crash.\r\n",
  "A sudden silence falls as the constant wind dies down, making the quiet feel heavy and oppressive.\r\n", // 120
  "depressing traffic messages index error - alert staff if you see this\r\n" // MUST BE LAST
};
#define NUM_DEPRESSING_MESSAGES 121

void regenerate_traffic_msgs() {
  for (int which = 0; which < NUM_TRAFFIC_MESSAGE_TYPES; which++) {
    int qty = (which == TRAFFIC_MESSAGES_NORMAL ? NUM_TRAFFIC_MESSAGES : NUM_DEPRESSING_MESSAGES);
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
      // Add canned traffic messages to the queue.
      for (int msg_idx = 0; msg_idx < qty; msg_idx++)
        queue.push_back(str_dup(messages[msg_idx]));

      // Add dynamic traffic messages to the queue.
      if (which != TRAFFIC_MESSAGES_DEPRESSING) {
        for (int dyn_idx = 0; dyn_idx < (int) (qty * 2); dyn_idx++)
          queue.push_back(generate_dynamic_traffic_message__returns_new());
      }

      // Shuffle it.
      std::random_device rd;
      std::mt19937 g(rd());

      std::shuffle(queue.begin(), queue.end(), g);
    }

    // Assign this loop's traffic message.
    delete [] current_traffic_msg[which];
    current_traffic_msg[which] = queue.back();
    queue.pop_back();
  }
}

SPECIAL(traffic)
{
  struct room_data *room = (struct room_data *) me;

  if (!cmd && room->people) {
    if (GET_JURISDICTION(room) == JURISDICTION_SECRET) {
      send_to_room(current_traffic_msg[TRAFFIC_MESSAGES_DEPRESSING], room, NULL, TRUE);
    } else {
      send_to_room(current_traffic_msg[TRAFFIC_MESSAGES_NORMAL], room, NULL, TRUE);
    }
  }

  return FALSE;
}