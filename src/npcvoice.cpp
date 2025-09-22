
#include "npcvoice.hpp"
#include "interpreter.hpp"
#include "db.hpp"
#include <unordered_map>
#include <ctime>
#include <cctype>
#include <string>

extern int cmd_say;
ACMD_DECLARE(do_say);

namespace {

struct TalkState {
  time_t next_ok = 0;
  time_t last_greet = 0, last_farewell = 0, last_listen = 0;
  time_t last_combat = 0, last_hit = 0, last_death = 0;
  time_t last_shop = 0, last_quest = 0, last_receive = 0;
  const char* last_greet_line = nullptr;
  const char* last_farewell_line = nullptr;
  const char* last_listen_line = nullptr;
  const char* last_combat_line = nullptr;
  const char* last_hit_line = nullptr;
  const char* last_death_line = nullptr;
  const char* last_shop_line = nullptr;
  const char* last_quest_line = nullptr;
  const char* last_recv_line = nullptr;
  bool in_combat = false;
};

static std::unordered_map<const void*, TalkState> g_states;
static std::unordered_map<const void*, int> g_forced_pers;

static inline bool chance(int pct) { return number(1,100) <= pct; }
static inline time_t nowts() { return time(0); }
static bool can_speak(const void* key) { return nowts() >= g_states[key].next_ok; }
static void arm_cooldown(const void* key) { g_states[key].next_ok = nowts() + number(10, 120); }

static void say_to(struct char_data* mob, struct char_data* ch, const char* msg) {
  if (!mob || !ch || !msg || !*msg) return;
  char buf[MAX_STRING_LENGTH];
  snprintf(buf, sizeof(buf), "%s", msg);
  do_say(mob, buf, cmd_say, SCMD_SAYTO);
}

static const char* pick_nonrepeat(const char** arr, int n, const char*& last_field) {
  if (!arr || n <= 0) return nullptr;
  const char* pick = arr[number(0, n-1)];
  if (last_field && n > 1) {
    for (int tries = 0; tries < 5 && pick == last_field; ++tries)
      pick = arr[number(0, n-1)];
  }
  last_field = pick;
  return pick;
}

static bool token_match(const char* text, const char* pattern) {
  if (!text || !pattern) return false;
  auto lower = [](std::string s){ for (auto &c: s) c = std::tolower((unsigned char)c); return s; };
  std::string t = lower(text);
  std::string p = lower(pattern);
  size_t start = 0;
  while (true) {
    size_t bar = p.find('|', start);
    std::string tok = p.substr(start, bar == std::string::npos ? std::string::npos : bar - start);
    if (!tok.empty() && t.find(tok) != std::string::npos) return true;
    if (bar == std::string::npos) break;
    start = bar + 1;
  }
  return false;
}

static std::string expand(const char* in, struct char_data* ch, struct char_data* mob) {
  std::string s = in ? in : "";
  if (ch) {
    const char* nm = GET_CHAR_NAME(ch);
    size_t pos;
    while ((pos = s.find("%name%")) != std::string::npos) s.replace(pos, 6, nm ? nm : "runner");
  }
  if (ch && ch->in_room && ch->in_room->name) {
    const char* rm = ch->in_room->name;
    size_t pos;
    while ((pos = s.find("%room%")) != std::string::npos) s.replace(pos, 6, rm);
  }
  return s;
}

} // anon

namespace NPCVoice {

void set_personality(struct char_data* mob, Personality p) { g_forced_pers[mob] = (int)p; }
Personality get_personality(struct char_data* mob) {
  auto it = g_forced_pers.find(mob);
  if (it != g_forced_pers.end()) return (Personality)it->second;
  long n = GET_NUYEN(mob);
  if (n <= 0) return (Personality)number(0,2);
  if (n <= 500) return PERS_GANG;
  if (n <= 5000) return PERS_NEUTRAL;
  return PERS_POLITE;
}

static const char* GREET_GANG[] = {
  "New face. advertising sings off-key. mind the cameras.",
  "Yo-yo. boots know the road. stay off the feeds.",
  "Yo, runner. looking for trouble or directions?.",
  "Sup, gunslinger. walk light; the concrete remembers.",
  "New face. rain tastes like copper. mind the cameras.",
  "Yo, gunslinger. walk light; the concrete remembers.",
  "Oi. window shopping’s free.",
  "Sup, chummer. city’s a blender—try not to be fruit.",
  "Yo. boots know the road. stay off the feeds.",
  "Sup, avatar. walk light; the concrete remembers.",
  "Hey. nice coat. don’t get iced.",
  "Yo. fast shadow today. don’t bleed on the merch.",
  "Yo. fast shadow today. stay off the feeds.",
  "You smell like burnt plastic. the floor collects trophies.",
  "Yo. your grin looks aftermarket. don’t bleed on the merch.",
  "Hey. your grin looks aftermarket. keep your cred tight.",
  "Oi. I accept tips in untraceable compliments.",
  "New face. the grid is loud. mind the cameras.",
  "Oi, chummer. looking for trouble or directions?.",
  "Hey, street saint. don’t make me memorize you the hard way.",
  "New face. sirens sound like lullabies. watch your six.",
  "You smell like fresh firmware. mind the cameras.",
  "Hey, chummer. city’s a blender—try not to be fruit.",
  "Sup, avatar. city’s a blender—try not to be fruit.",
  "Sup. your grin looks aftermarket. watch your six.",
  "Ayo, chummer. walk light; the concrete remembers.",
  "Ayo, runner. city’s a blender—try not to be fruit.",
  "Ayo. terms and conditions apply.",
  "Hey. your warranty’s void.",
  "Hey. no refunds on bad ideas.",
  "Oi. terms and conditions apply.",
  "New face. the grid is loud. watch your six.",
  "Ayo. fast shadow today. keep your cred tight.",
  "Ayo, avatar. don’t make me memorize you the hard way.",
  "New face. sirens sound like lullabies. don’t get iced.",
  "Sup. fast shadow today. keep your cred tight.",
  "Hey, chromeheart. looking for trouble or directions?.",
  "Oi, chromeheart. city’s a blender—try not to be fruit.",
  "Sup, ghost. walk light; the concrete remembers.",
  "Hey. smile for the streetcams.",
  "Yo. fast shadow today. don’t get iced.",
  "Yo-yo. smile for the streetcams.",
  "Yo-yo, chummer. walk light; the concrete remembers.",
  "You smell like cheap cologne. watch your six.",
  "Ayo. your grin looks aftermarket. keep your cred tight.",
  "Yo. smile for the streetcams.",
  "You smell like rain. stay off the feeds.",
  "New face. the grid is loud. keep your cred tight.",
  "Yo-yo. your grin looks aftermarket. don’t bleed on the merch.",
  "New face. the drones are awake. keep your cred tight.",
  "Yo, gunslinger. don’t make me memorize you the hard way.",
  "Hey, street saint. city’s a blender—try not to be fruit.",
  "Yo-yo. nice coat. don’t bleed on the merch.",
  "You smell like burnt plastic. don’t bleed on the merch.",
  "Ayo. smile for the streetcams.",
  "New face. the neon hums. stay off the feeds.",
  "Yo-yo. your warranty’s void.",
  "Sup. nice coat. keep your cred tight.",
  "Yo-yo, avatar. walk light; the concrete remembers.",
  "Yo-yo. your eyes look modded. mind the cameras.",
  "Hey. I accept tips in untraceable compliments.",
  "Yo, netrat. don’t make me memorize you the hard way.",
  "New face. the grid is loud. don’t get iced.",
  "Yo. window shopping’s free.",
  "New face. advertising sings off-key. the floor collects trophies.",
  "Ayo, tourist. walk light; the concrete remembers.",
  "Oi, tourist. looking for trouble or directions?.",
  "Ayo. boots know the road. mind the cameras.",
  "Yo. terms and conditions apply.",
  "Big city, small luck. Pick one.",
  "Oi, avatar. city’s a blender—try not to be fruit.",
  "You smell like burnt plastic. mind the cameras.",
  "Ayo, chromeheart. don’t make me memorize you the hard way.",
  "Hey, tourist. city’s a blender—try not to be fruit.",
  "Oi. boots know the road. watch your six.",
  "Hey. terms and conditions apply.",
  "Yo-yo. nice coat. don’t get iced.",
  "Yo, street saint. don’t make me memorize you the hard way.",
  "Sup, netrat. looking for trouble or directions?.",
  "You smell like spent casings. mind the cameras.",
  "New face. advertising sings off-key. don’t bleed on the merch.",
  "Yo. your grin looks aftermarket. keep your cred tight.",
  "Oi. your warranty’s void.",
  "Hey. your eyes look modded. the floor collects trophies.",
  "Ayo, runner. looking for trouble or directions?.",
  "Yo-yo, biohacker. city’s a blender—try not to be fruit.",
  "You smell like rain. mind the cameras.",
  "Oi, street saint. don’t make me memorize you the hard way.",
  "Yo-yo. nice coat. stay off the feeds.",
  "Ayo. no refunds on bad ideas.",
  "Yo-yo, runner. city’s a blender—try not to be fruit.",
  "New face. the grid is loud. don’t bleed on the merch.",
  "Yo. no refunds on bad ideas.",
  "You smell like fresh firmware. keep your cred tight.",
  "New face. the city’s hungry. mind the cameras.",
  "Yo, street saint. walk light; the concrete remembers.",
  "New face. the neon hums. keep your cred tight.",
  "Mask on, manners off—my kind of guest.",
  "Sup. smile for the streetcams.",
  "Ayo, ghost. don’t make me memorize you the hard way.",
  "You smell like burnt plastic. watch your six.",
  "You smell like spent casings. watch your six.",
  "Hey. nice coat. don’t bleed on the merch.",
  "Ayo. your grin looks aftermarket. don’t bleed on the merch.",
  "Ayo. pockets have rumors. keep your cred tight.",
  "Sup. your warranty’s void.",
  "New face. the drones are awake. don’t get iced.",
  "Ayo, tourist. don’t make me memorize you the hard way.",
  "Sup, chummer. don’t make me memorize you the hard way.",
  "You smell like ozone. watch your six.",
  "Yo-yo, ghost. don’t make me memorize you the hard way.",
  "New face. advertising sings off-key. watch your six.",
  "Yo-yo, netrat. don’t make me memorize you the hard way.",
  "You smell like rain. don’t bleed on the merch.",
  "You smell like rain. watch your six.",
  "Sup, chummer. walk light; the concrete remembers.",
  "Yo-yo. your grin looks aftermarket. watch your six.",
  "Ayo, tourist. looking for trouble or directions?.",
  "Hey. your eyes look modded. keep your cred tight.",
  "Yo-yo, netrat. walk light; the concrete remembers.",
  "Sup. boots know the road. don’t get iced.",
  "You smell like rain. don’t get iced.",
  "New face. the neon hums. don’t bleed on the merch.",
  "Yo-yo. nice coat. watch your six.",
  "Ayo, ghost. walk light; the concrete remembers.",
  "New face. rain tastes like copper. stay off the feeds.",
  "Hey, netrat. looking for trouble or directions?.",
  "You smell like cheap cologne. keep your cred tight.",
  "Sup, avatar. looking for trouble or directions?.",
  "You smell like rain. the floor collects trophies.",
  "Yo-yo. fast shadow today. keep your cred tight.",
  "Oi. no refunds on bad ideas.",
  "You smell like fresh firmware. stay off the feeds.",
  "New face. the city’s hungry. watch your six.",
  "Ayo, avatar. walk light; the concrete remembers.",
  "You smell like cheap cologne. mind the cameras.",
  "Ayo, netrat. looking for trouble or directions?.",
  "You smell like spent casings. stay off the feeds.",
  "Ayo, street saint. walk light; the concrete remembers.",
  "You smell like ozone. stay off the feeds.",
  "You smell like spent casings. don’t bleed on the merch.",
  "Ayo. your warranty’s void.",
  "New face. the city’s hungry. don’t get iced.",
  "New face. rain tastes like copper. watch your six.",
  "New face. the drones are awake. mind the cameras.",
  "Yo-yo. your eyes look modded. don’t get iced.",
  "Sup. I accept tips in untraceable compliments.",
  "Hey, biohacker. looking for trouble or directions?.",
  "You smell like burnt plastic. don’t get iced.",
  "Yo. boots know the road. the floor collects trophies.",
  "Yo, chromeheart. city’s a blender—try not to be fruit.",
  "New face. advertising sings off-key. don’t get iced.",
  "Yo. your warranty’s void.",
  "Hey. fast shadow today. don’t bleed on the merch.",
  "You smell like cheap cologne. don’t bleed on the merch.",
  "New face. sirens sound like lullabies. the floor collects trophies.",
  "You smell like cheap cologne. don’t get iced.",
  "You smell like fresh firmware. don’t get iced.",
  "Yo, ghost. looking for trouble or directions?.",
  "Hey. your grin looks aftermarket. watch your six."
};

static const char* GREET_NEUTRAL[] = {
  "Evenin'. window shopping’s free.",
  "Yo. pockets have rumors.",
  "You’re on time; the city’s hungry.",
  "Hi. window shopping’s free.",
  "Hi. your warranty’s void.",
  "Heat map says we’re chill—for now.",
  "Howdy. window shopping’s free.",
  "Yo. I accept tips in untraceable compliments.",
  "Welcome. your warranty’s void.",
  "Evenin'. I accept tips in untraceable compliments.",
  "Howdy. no refunds on bad ideas.",
  "Yo. Need directions, a tip, or quiet?",
  "Welcome. your grin looks aftermarket.",
  "Evenin'. smile for the streetcams.",
  "Take a breath—smells like old coffee and ozone.",
  "Evenin'. terms and conditions apply.",
  "Howdy. your warranty’s void.",
  "Yo. your grin looks aftermarket.",
  "Hi. pockets have rumors.",
  "You’re on time; the neon hums.",
  "Welcome. your eyes look modded.",
  "Hey. your warranty’s void.",
  "Hey. no refunds on bad ideas.",
  "Welcome. boots know the road.",
  "You’re on time; sirens sound like lullabies.",
  "You’re on time; advertising sings off-key.",
  "Yo. boots know the road.",
  "Howdy. your eyes look modded.",
  "Howdy. fast shadow today.",
  "Hey. smile for the streetcams.",
  "Hi. I accept tips in untraceable compliments.",
  "Hi. terms and conditions apply.",
  "Yo. smile for the streetcams.",
  "Welcome. nice coat.",
  "Hey. nice coat.",
  "Hey. pockets have rumors.",
  "You’re on time; rain tastes like copper.",
  "Howdy. I accept tips in untraceable compliments.",
  "Evenin'. your grin looks aftermarket.",
  "Welcome. pockets have rumors.",
  "Hey. I accept tips in untraceable compliments.",
  "Howdy. pockets have rumors.",
  "Evenin'. Welcome to %room%.",
  "Hey. window shopping’s free.",
  "Evenin'. your warranty’s void.",
  "Welcome. smile for the streetcams.",
  "Yo. window shopping’s free.",
  "Howdy. Need directions, a tip, or quiet?",
  "Welcome. I accept tips in untraceable compliments.",
  "Welcome. no refunds on bad ideas.",
  "Yo. terms and conditions apply.",
  "Hey. terms and conditions apply.",
  "Howdy. your grin looks aftermarket.",
  "Yo. your eyes look modded.",
  "Welcome. Need directions, a tip, or quiet?",
  "You’re on time; the grid is loud.",
  "Hi. smile for the streetcams.",
  "Evenin'. nice coat.",
  "Evenin'. boots know the road.",
  "Yo. no refunds on bad ideas.",
  "Hey. Welcome to %room%.",
  "Hey. Need directions, a tip, or quiet?",
  "Howdy. terms and conditions apply.",
  "Hey. your eyes look modded.",
  "Hi. fast shadow today.",
  "You’re on time; the drones are awake.",
  "Evenin'. no refunds on bad ideas.",
  "Take a breath—it smells like rain and solder.",
  "Hi. Need directions, a tip, or quiet?",
  "Welcome. terms and conditions apply.",
  "Take a breath—air tastes like batteries.",
  "Evenin'. your eyes look modded.",
  "Hi. nice coat.",
  "Hey. your grin looks aftermarket.",
  "Take a breath—filters complain about dust.",
  "Hi. your grin looks aftermarket.",
  "Welcome. fast shadow today.",
  "Howdy. smile for the streetcams.",
  "Howdy. nice coat.",
  "Yo. nice coat.",
  "Evenin'. pockets have rumors.",
  "Welcome. window shopping’s free.",
  "Hi. no refunds on bad ideas.",
  "Welcome. Welcome to %room%.",
  "Howdy. boots know the road.",
  "Hi. your eyes look modded.",
  "Plenty of exits, fewer entrances.",
  "Hey. fast shadow today.",
  "Howdy. Welcome to %room%.",
  "Yo. fast shadow today.",
  "Hi. boots know the road.",
  "Evenin'. fast shadow today.",
  "Evenin'. Need directions, a tip, or quiet?",
  "Yo. your warranty’s void.",
  "Hey. boots know the road.",
  "Yo. Welcome to %room%.",
  "Hi. Welcome to %room%."
};

static const char* GREET_POLITE[] = {
  "Welcome to %room%. Security with taste.",
  "Salutations, %name%. A pleasure.",
  "Welcome to %room%. Efficiency without cruelty.",
  "May I offer discretion?",
  "Delighted. We polished the neon just for you.",
  "Welcome to %room%. Do let me know if I can assist.",
  "Salutations. Compliments are complimentary today.",
  "Salutations. We polished the neon just for you.",
  "May I offer a map?",
  "Welcome to %room%. Discretion without delay.",
  "Delighted, %name%. A pleasure.",
  "Welcome, %name%. A pleasure.",
  "Welcome. We polished the neon just for you.",
  "Delighted. Compliments are complimentary today.",
  "Pleasure. Compliments are complimentary today.",
  "Greetings. Our drones know your good side.",
  "Pleasure. We polished the neon just for you.",
  "Welcome. Compliments are complimentary today.",
  "Your arrival improves the scenery.",
  "May I offer coffee?",
  "Delighted. Our drones know your good side.",
  "Salutations. Our drones know your good side.",
  "Good evening. Our drones know your good side.",
  "Greetings. Compliments are complimentary today.",
  "Good evening. We polished the neon just for you.",
  "Greetings, %name%. A pleasure.",
  "Pleasure, %name%. A pleasure.",
  "Welcome. Our drones know your good side.",
  "Allow me to recommend the safe exits—purely precautionary.",
  "Good evening. Compliments are complimentary today.",
  "Good evening, %name%. A pleasure.",
  "I trust your run proceeds smoothly?",
  "Greetings. We polished the neon just for you.",
  "Pleasure. Our drones know your good side.",
  "May I offer umbrella privileges?",
  "May I offer a kind lie?",
  "May I offer context?"
};

static const char* FAREWELL_GANG[] = {
  "Walk it off. stay off the feeds.",
  "Later. reload your luck.",
  "Later. stay frosty.",
  "Next time bring better stories.",
  "Later. don’t owe anyone I like.",
  "Later. don’t become a mural.",
  "Later. sprawl bites idle feet.",
  "Walk it off. don’t get iced.",
  "Walk it off. keep your cred tight.",
  "Walk it off. watch your six.",
  "Skedaddle before the drones get curious.",
  "Skedaddle before the grid notices.",
  "Walk it off. the floor collects trophies.",
  "Skedaddle before the cameras change mood.",
  "Later. keep moving.",
  "Next time bring more batteries.",
  "Don’t practice dying; you’ll get good at it.",
  "Call me if you need plausible deniability.",
  "Walk it off. don’t bleed on the merch.",
  "Walk it off. mind the cameras.",
  "Next time bring worse scars.",
  "Skedaddle before the alley grows teeth.",
  "Next time bring louder alibis."
};

static const char* FAREWELL_NEUTRAL[] = {
  "Catch you on the next loop.",
  "Take care. maps lie less than people.",
  "See you in the glow.",
  "Take care. mind the cameras.",
  "Take care. roads are moody.",
  "Take care. watch your back.",
  "Take care. exits are honest.",
  "Take care. keep your comms dry.",
  "Run quiet; run clever.",
  "Stay brave, stay boring—pick one.",
  "Rain’s coming; wear it well."
};

static const char* FAREWELL_POLITE[] = {
  "Best of luck; accept none from strangers.",
  "Consider me an ally of aesthetics.",
  "May the grid guide your steps.",
  "Until next time—stay luminous.",
  "Safe travels, %name%.",
  "I shall remain attentive in your absence."
};

static const char* L_HELLO_GANG[] = {
  "What’s buzzin’, heat’s on.",
  "What’s buzzin’.",
  "Spit it, time is nuyen.",
  "Talk to me. make it sharp.",
  "Five seconds. time is nuyen.",
  "Yo. heat’s on.",
  "Talk to me time is nuyen.",
  "Yo, heat’s on.",
  "Five seconds time is nuyen.",
  "Talk to me.",
  "Five seconds, don’t waste signal.",
  "What’s buzzin’, don’t waste signal.",
  "Spit it, make it sharp.",
  "Yo. make it sharp.",
  "Talk to me heat’s on.",
  "Spit it don’t waste signal.",
  "Five seconds.",
  "What’s buzzin’. don’t waste signal.",
  "Yo time is nuyen.",
  "Talk to me. time is nuyen.",
  "Five seconds. don’t waste signal.",
  "Talk to me. don’t waste signal.",
  "Spit it. make it sharp.",
  "Spit it. don’t waste signal.",
  "What’s buzzin’ heat’s on.",
  "Yo, don’t waste signal.",
  "Talk to me. heat’s on.",
  "Five seconds. heat’s on.",
  "Yo. don’t waste signal.",
  "What’s buzzin’, time is nuyen.",
  "Talk to me, heat’s on.",
  "Spit it. heat’s on.",
  "Five seconds make it sharp.",
  "Talk to me make it sharp.",
  "Spit it. time is nuyen.",
  "What’s buzzin’ time is nuyen.",
  "What’s buzzin’ make it sharp.",
  "Yo don’t waste signal.",
  "Spit it make it sharp.",
  "Talk to me, don’t waste signal.",
  "Five seconds don’t waste signal.",
  "Yo, make it sharp.",
  "What’s buzzin’. make it sharp.",
  "Spit it heat’s on.",
  "What’s buzzin’ don’t waste signal.",
  "Spit it.",
  "Spit it time is nuyen.",
  "Yo heat’s on.",
  "Yo, time is nuyen.",
  "Talk to me, make it sharp.",
  "Spit it, heat’s on.",
  "What’s buzzin’. heat’s on.",
  "Yo.",
  "Yo. time is nuyen.",
  "Five seconds, time is nuyen.",
  "Spit it, don’t waste signal.",
  "What’s buzzin’. time is nuyen.",
  "Five seconds. make it sharp.",
  "Talk to me, time is nuyen.",
  "Five seconds heat’s on.",
  "Talk to me don’t waste signal.",
  "What’s buzzin’, make it sharp.",
  "Five seconds, heat’s on.",
  "Five seconds, make it sharp.",
  "Yo make it sharp."
};

static const char* L_HELLO_NEU[] = {
  "Listening. I’m here.",
  "What’s up. shoot.",
  "Go on. make it quick.",
  "Hey I’m all ears.",
  "Hey make it quick.",
  "Evenin' make it quick.",
  "Hey shoot.",
  "What’s up, shoot.",
  "Listening. make it quick.",
  "Evenin'. I’m all ears.",
  "What’s up, I’m all ears.",
  "Hey, I’m here.",
  "Hey, shoot.",
  "Go on make it quick.",
  "Evenin'. make it quick.",
  "Listening I’m here.",
  "Go on I’m here.",
  "Listening. shoot.",
  "Evenin'. I’m here.",
  "Hey. I’m all ears.",
  "What’s up shoot.",
  "What’s up I’m all ears.",
  "Go on. I’m here.",
  "Evenin' I’m here.",
  "Listening. I’m all ears.",
  "Go on. I’m all ears.",
  "What’s up. I’m all ears.",
  "Evenin', make it quick.",
  "Hey. I’m here.",
  "Go on shoot.",
  "What’s up I’m here.",
  "What’s up. make it quick.",
  "Listening, I’m here.",
  "What’s up. I’m here.",
  "Hey, I’m all ears.",
  "Evenin' shoot.",
  "Listening, make it quick.",
  "What’s up.",
  "Go on I’m all ears.",
  "Listening make it quick.",
  "Evenin'. shoot.",
  "Listening, shoot.",
  "Evenin', I’m here.",
  "Listening, I’m all ears.",
  "What’s up, make it quick.",
  "Hey I’m here.",
  "Hey. make it quick.",
  "Go on, I’m all ears.",
  "Hey.",
  "Evenin'.",
  "Go on, I’m here.",
  "Hey, make it quick.",
  "Go on, shoot.",
  "What’s up make it quick.",
  "Go on.",
  "Evenin' I’m all ears.",
  "Listening.",
  "Go on, make it quick.",
  "Evenin', shoot.",
  "What’s up, I’m here.",
  "Go on. shoot.",
  "Listening shoot.",
  "Hey. shoot.",
  "Evenin', I’m all ears.",
  "Listening I’m all ears."
};

static const char* L_HELLO_POL[] = {
  "Speak freely be precise.",
  "Yes? do elaborate.",
  "Yes?, how may I assist.",
  "Hello there.",
  "Delighted, be precise.",
  "Delighted. how may I assist.",
  "Hello there, I’m listening kindly.",
  "Hello there, how may I assist.",
  "You have my attention, do elaborate.",
  "Speak freely how may I assist.",
  "You have my attention, be precise.",
  "You have my attention, I’m listening kindly.",
  "You have my attention. how may I assist.",
  "Hello there. how may I assist.",
  "Yes? how may I assist.",
  "You have my attention, how may I assist.",
  "You have my attention how may I assist.",
  "Hello there, do elaborate.",
  "You have my attention.",
  "Hello there how may I assist.",
  "Yes?. I’m listening kindly.",
  "You have my attention. do elaborate.",
  "You have my attention. I’m listening kindly.",
  "Speak freely. do elaborate.",
  "Delighted I’m listening kindly.",
  "Speak freely, I’m listening kindly.",
  "Yes?, be precise.",
  "Delighted. be precise.",
  "Hello there I’m listening kindly.",
  "You have my attention I’m listening kindly.",
  "Yes?, do elaborate.",
  "Yes?. how may I assist.",
  "Delighted how may I assist.",
  "Yes?, I’m listening kindly.",
  "Yes?. be precise.",
  "Hello there. do elaborate.",
  "Delighted. I’m listening kindly.",
  "Delighted.",
  "Yes?. do elaborate.",
  "Hello there be precise.",
  "Speak freely do elaborate.",
  "Hello there, be precise.",
  "Delighted, do elaborate.",
  "Hello there. I’m listening kindly.",
  "Delighted, how may I assist.",
  "Yes? I’m listening kindly.",
  "Speak freely, be precise.",
  "Delighted do elaborate.",
  "You have my attention do elaborate.",
  "Yes?",
  "Speak freely, do elaborate.",
  "Delighted. do elaborate.",
  "Delighted, I’m listening kindly.",
  "You have my attention be precise.",
  "Yes? be precise.",
  "Hello there do elaborate.",
  "Speak freely. I’m listening kindly.",
  "Delighted be precise.",
  "Speak freely. be precise.",
  "You have my attention. be precise.",
  "Speak freely I’m listening kindly.",
  "Speak freely, how may I assist.",
  "Hello there. be precise.",
  "Speak freely.",
  "Speak freely. how may I assist."
};

static const char* L_HELP_GANG[] = {
  "Advice or alibi answers ain’t free.",
  "Help costs nuyen, no guarantees.",
  "Advice or alibi. no guarantees.",
  "Maybe I know a guy no guarantees.",
  "Maybe I know a guy.",
  "Maybe I know a guy. no guarantees.",
  "Maybe I know a guy, I barter in questions.",
  "Help costs nuyen. no guarantees.",
  "Maybe I know a guy answers ain’t free.",
  "Help costs nuyen I barter in questions.",
  "Advice or alibi, answers ain’t free.",
  "Maybe I know a guy. I barter in questions.",
  "Advice or alibi. answers ain’t free.",
  "Maybe I know a guy. answers ain’t free.",
  "Help costs nuyen.",
  "Advice or alibi, I barter in questions.",
  "Advice or alibi.",
  "Advice or alibi. I barter in questions.",
  "Help costs nuyen, I barter in questions.",
  "Advice or alibi, no guarantees.",
  "Maybe I know a guy I barter in questions.",
  "Help costs nuyen. I barter in questions.",
  "Help costs nuyen. answers ain’t free.",
  "Maybe I know a guy, no guarantees.",
  "Maybe I know a guy, answers ain’t free.",
  "Advice or alibi no guarantees.",
  "Help costs nuyen no guarantees.",
  "Help costs nuyen, answers ain’t free.",
  "Advice or alibi I barter in questions.",
  "Help costs nuyen answers ain’t free."
};

static const char* L_HELP_NEU[] = {
  "Try me, be specific.",
  "Try me, we’ll find a way.",
  "I can point you, be specific.",
  "What d’you need, we’ll find a way.",
  "What d’you need. be specific.",
  "Try me be specific.",
  "What d’you need. if I know it.",
  "What d’you need if I know it.",
  "What d’you need, if I know it.",
  "What d’you need.",
  "I can point you. be specific.",
  "I can point you if I know it.",
  "Try me, if I know it.",
  "Try me. we’ll find a way.",
  "I can point you.",
  "I can point you, we’ll find a way.",
  "I can point you. if I know it.",
  "What d’you need, be specific.",
  "Try me.",
  "Try me. be specific.",
  "I can point you, if I know it.",
  "What d’you need be specific.",
  "I can point you be specific.",
  "Try me we’ll find a way.",
  "I can point you. we’ll find a way.",
  "I can point you we’ll find a way.",
  "Try me if I know it.",
  "What d’you need we’ll find a way.",
  "Try me. if I know it.",
  "What d’you need. we’ll find a way."
};

static const char* L_HELP_POL[] = {
  "Permit me a suggestion. within my remit.",
  "Happy to help. precision helps.",
  "Happy to help, precision helps.",
  "Permit me a suggestion precision helps.",
  "Happy to help. within my remit.",
  "Permit me a suggestion within my remit.",
  "Happy to help, consider me invested.",
  "Happy to help.",
  "Permit me a suggestion consider me invested.",
  "Of course.",
  "Permit me a suggestion.",
  "Of course. precision helps.",
  "Permit me a suggestion. precision helps.",
  "Of course within my remit.",
  "Permit me a suggestion, precision helps.",
  "Happy to help, within my remit.",
  "Of course, within my remit.",
  "Of course, precision helps.",
  "Happy to help within my remit.",
  "Of course precision helps.",
  "Of course, consider me invested.",
  "Of course consider me invested.",
  "Happy to help precision helps.",
  "Permit me a suggestion, within my remit.",
  "Permit me a suggestion. consider me invested.",
  "Permit me a suggestion, consider me invested.",
  "Of course. consider me invested.",
  "Happy to help consider me invested.",
  "Of course. within my remit.",
  "Happy to help. consider me invested."
};

static const char* L_QUEST_GANG[] = {
  "Fixer two blocks south, bring proof.",
  "Jobs?",
  "Fixer two blocks south. someone’s paying.",
  "Jobs?. someone’s paying.",
  "Fixer two blocks south keep comms dark.",
  "Jobs?, someone’s paying.",
  "Fixer two blocks south bring proof.",
  "Wet work pays someone’s paying.",
  "Wet work pays, someone’s paying.",
  "Jobs?, keep comms dark.",
  "Wet work pays. bring proof.",
  "Jobs?. keep comms dark.",
  "Wet work pays. someone’s paying.",
  "Wet work pays, keep comms dark.",
  "Wet work pays bring proof.",
  "Fixer two blocks south, someone’s paying.",
  "Fixer two blocks south someone’s paying.",
  "Fixer two blocks south. bring proof.",
  "Fixer two blocks south. keep comms dark.",
  "Fixer two blocks south, keep comms dark.",
  "Jobs? someone’s paying.",
  "Jobs?, bring proof.",
  "Wet work pays, bring proof.",
  "Jobs? keep comms dark.",
  "Wet work pays. keep comms dark.",
  "Wet work pays.",
  "Fixer two blocks south.",
  "Wet work pays keep comms dark.",
  "Jobs?. bring proof.",
  "Jobs? bring proof."
};

static const char* L_QUEST_NEU[] = {
  "Heard whispers market kiosks blink.",
  "Heard whispers undernet chats.",
  "Try the terminals, I might route you.",
  "Johnson at the bar. market kiosks blink.",
  "Johnson at the bar undernet chats.",
  "Heard whispers, I might route you.",
  "Heard whispers. undernet chats.",
  "Try the terminals, undernet chats.",
  "Heard whispers I might route you.",
  "Heard whispers. I might route you.",
  "Johnson at the bar, undernet chats.",
  "Try the terminals, market kiosks blink.",
  "Johnson at the bar. undernet chats.",
  "Try the terminals I might route you.",
  "Johnson at the bar. I might route you.",
  "Try the terminals.",
  "Try the terminals. undernet chats.",
  "Johnson at the bar market kiosks blink.",
  "Try the terminals. market kiosks blink.",
  "Johnson at the bar.",
  "Heard whispers, undernet chats.",
  "Heard whispers.",
  "Heard whispers. market kiosks blink.",
  "Johnson at the bar I might route you.",
  "Try the terminals. I might route you.",
  "Johnson at the bar, I might route you.",
  "Try the terminals market kiosks blink.",
  "Try the terminals undernet chats.",
  "Johnson at the bar, market kiosks blink.",
  "Heard whispers, market kiosks blink."
};

static const char* L_QUEST_POL[] = {
  "I can make introductions, for respectable clientele.",
  "I can make introductions your talents fit.",
  "Opportunities abound, with discretion.",
  "I can make introductions, with discretion.",
  "Opportunities abound. for respectable clientele.",
  "I can make introductions, your talents fit.",
  "I’ll curate something for respectable clientele.",
  "I can make introductions. your talents fit.",
  "Opportunities abound for respectable clientele.",
  "Opportunities abound your talents fit.",
  "Opportunities abound. with discretion.",
  "Opportunities abound, your talents fit.",
  "I’ll curate something your talents fit.",
  "I’ll curate something. your talents fit.",
  "Opportunities abound.",
  "I’ll curate something.",
  "I can make introductions. for respectable clientele.",
  "I can make introductions. with discretion.",
  "Opportunities abound with discretion.",
  "I can make introductions with discretion.",
  "I can make introductions.",
  "I’ll curate something. with discretion.",
  "I’ll curate something, for respectable clientele.",
  "I’ll curate something, with discretion.",
  "I can make introductions for respectable clientele.",
  "I’ll curate something. for respectable clientele.",
  "Opportunities abound. your talents fit.",
  "I’ll curate something with discretion.",
  "I’ll curate something, your talents fit.",
  "Opportunities abound, for respectable clientele."
};

static const char* L_THANKS_GANG[] = {
  "We square, keep moving.",
  "We square owe me a better story.",
  "Tell nobody keep moving.",
  "Tell nobody. keep moving.",
  "Yeah yeah. keep moving.",
  "We square. owe me a better story.",
  "Tell nobody, keep moving.",
  "Yeah yeah. spend it.",
  "Tell nobody, owe me a better story.",
  "Yeah yeah keep moving.",
  "We square, owe me a better story.",
  "Yeah yeah, spend it.",
  "Tell nobody.",
  "Yeah yeah, keep moving.",
  "We square keep moving.",
  "Tell nobody, spend it.",
  "Tell nobody. owe me a better story.",
  "We square spend it.",
  "We square, spend it.",
  "We square. spend it.",
  "Tell nobody. spend it.",
  "Tell nobody owe me a better story.",
  "Yeah yeah spend it.",
  "Yeah yeah, owe me a better story.",
  "We square. keep moving.",
  "Tell nobody spend it.",
  "Yeah yeah. owe me a better story.",
  "Yeah yeah owe me a better story.",
  "Yeah yeah.",
  "We square."
};

static const char* L_THANKS_NEU[] = {
  "Anytime, carry on.",
  "All good.",
  "All good sure thing.",
  "No worries carry on.",
  "No worries.",
  "No worries you’re fine.",
  "No worries. carry on.",
  "All good. carry on.",
  "Anytime.",
  "Anytime, sure thing.",
  "Anytime. you’re fine.",
  "Anytime you’re fine.",
  "No worries. sure thing.",
  "Anytime sure thing.",
  "All good, you’re fine.",
  "No worries, carry on.",
  "Anytime. sure thing.",
  "No worries, sure thing.",
  "No worries sure thing.",
  "Anytime. carry on.",
  "All good carry on.",
  "No worries. you’re fine.",
  "All good. sure thing.",
  "All good you’re fine.",
  "All good, carry on.",
  "Anytime carry on.",
  "All good. you’re fine.",
  "No worries, you’re fine.",
  "Anytime, you’re fine.",
  "All good, sure thing."
};

static const char* L_THANKS_POL[] = {
  "Anytime, %name%, gracious as ever.",
  "You’re most welcome, consider me at your service.",
  "Anytime, %name%. gracious as ever.",
  "You’re most welcome, gracious as ever.",
  "You’re most welcome. consider me at your service.",
  "You’re most welcome. gracious as ever.",
  "The pleasure is mine gracious as ever.",
  "The pleasure is mine. consider me at your service.",
  "The pleasure is mine. gracious as ever.",
  "You’re most welcome very kind.",
  "Anytime, %name%. very kind.",
  "The pleasure is mine. very kind.",
  "The pleasure is mine, very kind.",
  "You’re most welcome, very kind.",
  "Anytime, %name%.",
  "You’re most welcome consider me at your service.",
  "The pleasure is mine, gracious as ever.",
  "The pleasure is mine, consider me at your service.",
  "Anytime, %name% very kind.",
  "Anytime, %name%, very kind.",
  "The pleasure is mine consider me at your service.",
  "You’re most welcome gracious as ever.",
  "The pleasure is mine very kind.",
  "Anytime, %name% consider me at your service.",
  "You’re most welcome.",
  "Anytime, %name% gracious as ever.",
  "The pleasure is mine.",
  "Anytime, %name%. consider me at your service.",
  "You’re most welcome. very kind.",
  "Anytime, %name%, consider me at your service."
};

static const char* C_START_GANG[] = {
  "Hope your docwagon’s paid. smile for the streetcams. void where prohibited.",
  "Step wrong and be art. terms and conditions apply. no refunds.",
  "Try me, tourist. smile for the streetcams. no refunds.",
  "Your warranty’s void. I collect regrets. fine print bites.",
  "Your warranty’s void. time to get abstract. void where prohibited.",
  "Your warranty’s void. smile for the streetcams. void where prohibited.",
  "Try me, tourist. I collect regrets. fine print bites.",
  "Step wrong and be art. street eats loud.",
  "Try me, tourist. smile for the streetcams. void where prohibited.",
  "Step wrong and be art. smile for the streetcams. no refunds.",
  "Try me, tourist.",
  "Your warranty’s void. street eats loud. void where prohibited.",
  "Step wrong and be art. smile for the streetcams.",
  "Step wrong and be art. smile for the streetcams. fine print bites.",
  "Hope your docwagon’s paid. I collect regrets. void where prohibited.",
  "Let’s dance. time to get abstract.",
  "Step wrong and be art. I collect regrets.",
  "Hope your docwagon’s paid. street eats loud. no refunds.",
  "Hope your docwagon’s paid. I collect regrets. fine print bites.",
  "Try me, tourist. time to get abstract. no refunds.",
  "Try me, tourist. street eats loud.",
  "Let’s dance. street eats loud. no refunds.",
  "Let’s dance. smile for the streetcams. void where prohibited.",
  "Step wrong and be art. I collect regrets. fine print bites.",
  "Hope your docwagon’s paid. time to get abstract. void where prohibited.",
  "Your warranty’s void. terms and conditions apply.",
  "Let’s dance. smile for the streetcams.",
  "Try me, tourist. terms and conditions apply. fine print bites.",
  "Step wrong and be art. time to get abstract. void where prohibited.",
  "Let’s dance. I collect regrets. fine print bites.",
  "Let’s dance. street eats loud.",
  "Try me, tourist. I collect regrets. void where prohibited.",
  "Hope your docwagon’s paid. street eats loud. void where prohibited.",
  "Step wrong and be art. time to get abstract.",
  "Your warranty’s void. terms and conditions apply. fine print bites.",
  "Try me, tourist. terms and conditions apply. no refunds.",
  "Your warranty’s void. street eats loud. fine print bites.",
  "Hope your docwagon’s paid. smile for the streetcams. fine print bites.",
  "Your warranty’s void. smile for the streetcams.",
  "Try me, tourist. smile for the streetcams. fine print bites.",
  "Try me, tourist. time to get abstract. void where prohibited.",
  "Hope your docwagon’s paid. smile for the streetcams. no refunds.",
  "Your warranty’s void. street eats loud.",
  "Let’s dance. terms and conditions apply.",
  "Step wrong and be art. street eats loud. no refunds.",
  "Try me, tourist. terms and conditions apply.",
  "Let’s dance. time to get abstract. void where prohibited.",
  "Let’s dance. street eats loud. void where prohibited.",
  "Let’s dance. street eats loud. fine print bites.",
  "Hope your docwagon’s paid. terms and conditions apply. void where prohibited.",
  "Hope your docwagon’s paid. time to get abstract.",
  "Hope your docwagon’s paid. I collect regrets.",
  "Your warranty’s void. I collect regrets. void where prohibited.",
  "Hope your docwagon’s paid. I collect regrets. no refunds.",
  "Your warranty’s void. time to get abstract. no refunds.",
  "Hope your docwagon’s paid. time to get abstract. no refunds.",
  "Hope your docwagon’s paid. terms and conditions apply.",
  "Step wrong and be art. smile for the streetcams. void where prohibited.",
  "Your warranty’s void. terms and conditions apply. no refunds.",
  "Step wrong and be art. street eats loud. void where prohibited.",
  "Step wrong and be art. terms and conditions apply. void where prohibited.",
  "Let’s dance. smile for the streetcams. no refunds.",
  "Try me, tourist. time to get abstract. fine print bites.",
  "Let’s dance. terms and conditions apply. no refunds.",
  "Let’s dance. I collect regrets. no refunds.",
  "Your warranty’s void. I collect regrets.",
  "Hope your docwagon’s paid. smile for the streetcams.",
  "Step wrong and be art. I collect regrets. void where prohibited.",
  "Let’s dance. terms and conditions apply. fine print bites.",
  "Try me, tourist. time to get abstract.",
  "Your warranty’s void. smile for the streetcams. no refunds.",
  "Hope your docwagon’s paid. terms and conditions apply. no refunds.",
  "Try me, tourist. street eats loud. no refunds.",
  "Let’s dance. time to get abstract. no refunds.",
  "Hope your docwagon’s paid. time to get abstract. fine print bites.",
  "Your warranty’s void. street eats loud. no refunds.",
  "Step wrong and be art. time to get abstract. fine print bites.",
  "Try me, tourist. street eats loud. void where prohibited.",
  "Hope your docwagon’s paid. street eats loud.",
  "Try me, tourist. I collect regrets.",
  "Your warranty’s void.",
  "Hope your docwagon’s paid. terms and conditions apply. fine print bites.",
  "Hope your docwagon’s paid.",
  "Your warranty’s void. time to get abstract.",
  "Step wrong and be art. terms and conditions apply.",
  "Your warranty’s void. time to get abstract. fine print bites.",
  "Your warranty’s void. smile for the streetcams. fine print bites.",
  "Hope your docwagon’s paid. street eats loud. fine print bites.",
  "Try me, tourist. street eats loud. fine print bites.",
  "Try me, tourist. smile for the streetcams.",
  "Your warranty’s void. I collect regrets. no refunds.",
  "Step wrong and be art. time to get abstract. no refunds.",
  "Let’s dance. smile for the streetcams. fine print bites.",
  "Step wrong and be art. street eats loud. fine print bites.",
  "Try me, tourist. I collect regrets. no refunds.",
  "Let’s dance. I collect regrets.",
  "Try me, tourist. terms and conditions apply. void where prohibited.",
  "Let’s dance. terms and conditions apply. void where prohibited.",
  "Step wrong and be art. terms and conditions apply. fine print bites.",
  "Your warranty’s void. terms and conditions apply. void where prohibited.",
  "Step wrong and be art. I collect regrets. no refunds.",
  "Let’s dance. time to get abstract. fine print bites.",
  "Let’s dance. I collect regrets. void where prohibited.",
  "Step wrong and be art.",
  "Let’s dance."
};

static const char* C_START_NEU[] = {
  "Be quick about it. you sure about this.",
  "If this must be done. no heroes today.",
  "We can still walk away. no heroes today.",
  "Okay, blades out. not my preference. last chance.",
  "Be quick about it. no heroes today. last chance.",
  "If this must be done. no heroes today. this ends soon.",
  "Okay, blades out.",
  "If this must be done. not my preference.",
  "If this must be done. no heroes today. last chance.",
  "Okay, blades out. no heroes today.",
  "Be quick about it. you sure about this. last chance.",
  "We can still walk away. not my preference. this ends soon.",
  "Be quick about it. I warned you.",
  "If this must be done. you sure about this.",
  "We can still walk away. no heroes today. last chance.",
  "We can still walk away. you sure about this. last chance.",
  "Be quick about it. no heroes today.",
  "We can still walk away. I warned you. this ends soon.",
  "If this must be done.",
  "We can still walk away. not my preference.",
  "We can still walk away. you sure about this.",
  "If this must be done. not my preference. last chance.",
  "Be quick about it. no heroes today. this ends soon.",
  "Be quick about it. I warned you. last chance.",
  "Okay, blades out. I warned you. this ends soon.",
  "Okay, blades out. you sure about this. this ends soon.",
  "Be quick about it. not my preference. this ends soon.",
  "Okay, blades out. not my preference. this ends soon.",
  "Be quick about it.",
  "Okay, blades out. you sure about this.",
  "Okay, blades out. I warned you. last chance.",
  "If this must be done. not my preference. this ends soon.",
  "Okay, blades out. no heroes today. last chance.",
  "Be quick about it. not my preference. last chance.",
  "Okay, blades out. not my preference.",
  "Okay, blades out. you sure about this. last chance.",
  "If this must be done. you sure about this. this ends soon.",
  "Be quick about it. not my preference.",
  "Okay, blades out. no heroes today. this ends soon.",
  "If this must be done. I warned you.",
  "We can still walk away.",
  "Okay, blades out. I warned you.",
  "If this must be done. I warned you. this ends soon.",
  "If this must be done. you sure about this. last chance.",
  "We can still walk away. you sure about this. this ends soon.",
  "We can still walk away. no heroes today. this ends soon.",
  "We can still walk away. not my preference. last chance.",
  "Be quick about it. I warned you. this ends soon.",
  "Be quick about it. you sure about this. this ends soon.",
  "If this must be done. I warned you. last chance.",
  "We can still walk away. I warned you. last chance.",
  "We can still walk away. I warned you."
};

static const char* C_START_POL[] = {
  "Let’s be professional. let’s remain civil.",
  "Let’s be professional. no unnecessary cruelty. shall we.",
  "A tidy duel, please. do mind your posture. swiftly then.",
  "Let’s be professional. let’s remain civil. no mess.",
  "Regrettable, but necessary. elegance even now. no mess.",
  "En garde, %name%. do mind your posture. swiftly then.",
  "Let’s be professional. elegance even now. no mess.",
  "En garde, %name%. let’s remain civil.",
  "Regrettable, but necessary. elegance even now. shall we.",
  "Let’s be professional. do mind your posture.",
  "A tidy duel, please. let’s remain civil. swiftly then.",
  "En garde, %name%. do mind your posture.",
  "Let’s be professional. elegance even now. shall we.",
  "Regrettable, but necessary. do mind your posture. shall we.",
  "A tidy duel, please. let’s remain civil.",
  "En garde, %name%.",
  "Let’s be professional. let’s remain civil. swiftly then.",
  "Regrettable, but necessary. no unnecessary cruelty.",
  "Let’s be professional. no unnecessary cruelty. no mess.",
  "A tidy duel, please. do mind your posture. no mess.",
  "Regrettable, but necessary. let’s remain civil. shall we.",
  "Regrettable, but necessary. do mind your posture. no mess.",
  "Regrettable, but necessary. let’s remain civil.",
  "Regrettable, but necessary.",
  "En garde, %name%. no unnecessary cruelty. swiftly then.",
  "A tidy duel, please. elegance even now. shall we.",
  "En garde, %name%. elegance even now.",
  "Regrettable, but necessary. let’s remain civil. no mess.",
  "Regrettable, but necessary. do mind your posture. swiftly then.",
  "En garde, %name%. elegance even now. swiftly then.",
  "Regrettable, but necessary. elegance even now. swiftly then.",
  "Let’s be professional. do mind your posture. no mess.",
  "En garde, %name%. do mind your posture. shall we.",
  "Let’s be professional. let’s remain civil. shall we.",
  "Regrettable, but necessary. no unnecessary cruelty. no mess.",
  "A tidy duel, please. no unnecessary cruelty.",
  "A tidy duel, please. elegance even now. no mess.",
  "A tidy duel, please. no unnecessary cruelty. no mess.",
  "Let’s be professional. do mind your posture. swiftly then.",
  "Let’s be professional. elegance even now.",
  "En garde, %name%. do mind your posture. no mess.",
  "Regrettable, but necessary. do mind your posture.",
  "Regrettable, but necessary. no unnecessary cruelty. swiftly then.",
  "En garde, %name%. elegance even now. no mess.",
  "En garde, %name%. let’s remain civil. no mess.",
  "Let’s be professional. elegance even now. swiftly then.",
  "A tidy duel, please. no unnecessary cruelty. swiftly then.",
  "Let’s be professional.",
  "Let’s be professional. no unnecessary cruelty. swiftly then.",
  "Let’s be professional. no unnecessary cruelty.",
  "En garde, %name%. no unnecessary cruelty.",
  "A tidy duel, please. do mind your posture. shall we.",
  "Regrettable, but necessary. no unnecessary cruelty. shall we.",
  "A tidy duel, please. no unnecessary cruelty. shall we.",
  "En garde, %name%. let’s remain civil. shall we.",
  "A tidy duel, please. let’s remain civil. no mess.",
  "En garde, %name%. no unnecessary cruelty. no mess.",
  "Regrettable, but necessary. elegance even now.",
  "A tidy duel, please. elegance even now.",
  "A tidy duel, please. elegance even now. swiftly then.",
  "A tidy duel, please. do mind your posture.",
  "En garde, %name%. elegance even now. shall we.",
  "En garde, %name%. no unnecessary cruelty. shall we.",
  "Let’s be professional. do mind your posture. shall we.",
  "Regrettable, but necessary. let’s remain civil. swiftly then.",
  "A tidy duel, please.",
  "En garde, %name%. let’s remain civil. swiftly then.",
  "A tidy duel, please. let’s remain civil. shall we."
};

static const char* C_HIT_GANG[] = {
  "Missed a spot. keep swinging.",
  "Crunch. keep swinging.",
  "You leak pretty. keep swinging.",
  "Crunch. laundry day was worse.",
  "You leak pretty. make it cinematic.",
  "Bleed for me. make it cinematic.",
  "You leak pretty.",
  "Missed a spot. laundry day was worse.",
  "Bleed for me.",
  "You leak pretty. laundry day was worse.",
  "Missed a spot.",
  "Crunch.",
  "Crunch. make it cinematic.",
  "Ouch—for you. keep swinging.",
  "Bleed for me. laundry day was worse.",
  "Ouch—for you.",
  "Ouch—for you. make it cinematic.",
  "Bleed for me. keep swinging.",
  "Missed a spot. make it cinematic.",
  "Ouch—for you. laundry day was worse."
};

static const char* C_HIT_NEU[] = {
  "Noted.",
  "Connected.",
  "Careful.",
  "Connected. mind your footing.",
  "Solid hit. mm.",
  "Solid hit.",
  "Watch it.",
  "Watch it. mind your footing.",
  "Noted. mm.",
  "Watch it. mm.",
  "Connected. mm.",
  "Careful. mm.",
  "Noted. mind your footing.",
  "Solid hit. mind your footing.",
  "Careful. mind your footing."
};

static const char* C_HIT_POL[] = {
  "Well placed.",
  "Touché.",
  "Pardon the bruise. as expected.",
  "Touché. refined aim.",
  "A clean strike. refined aim.",
  "Touché. as expected.",
  "Well placed. refined aim.",
  "Acceptable contact.",
  "Pardon the bruise. refined aim.",
  "Acceptable contact. as expected.",
  "A clean strike. as expected.",
  "Acceptable contact. refined aim.",
  "Well placed. as expected.",
  "A clean strike.",
  "Pardon the bruise."
};

static const char* C_DEATH_GANG[] = {
  "Another notch.",
  "Street eats well tonight. irony served.",
  "Another notch. irony served.",
  "Tell the grid I said hi. irony served.",
  "Shoulda stayed home. unsigned exit.",
  "Street eats well tonight. unsigned exit.",
  "Street eats well tonight.",
  "Tell the grid I said hi.",
  "Another notch. unsigned exit.",
  "Tell the grid I said hi. unsigned exit.",
  "Shoulda stayed home.",
  "Shoulda stayed home. irony served.",
  "Delete yourself harder next time. unsigned exit.",
  "Delete yourself harder next time. irony served.",
  "Delete yourself harder next time."
};

static const char* C_DEATH_NEU[] = {
  "Noise stops here.",
  "Over and done.",
  "That’s that.",
  "That’s that. paid in full.",
  "Sprawl takes who it wants. cycle ends.",
  "That’s that. cycle ends.",
  "Rest easy. cycle ends.",
  "Over and done. paid in full.",
  "Over and done. cycle ends.",
  "Sprawl takes who it wants. paid in full.",
  "Noise stops here. cycle ends.",
  "Rest easy.",
  "Sprawl takes who it wants.",
  "Rest easy. paid in full.",
  "Noise stops here. paid in full."
};

static const char* C_DEATH_POL[] = {
  "Apologies it came to this.",
  "We are concluded. quiet now.",
  "May you find cleaner streets. dignity retained.",
  "May you find cleaner streets.",
  "Farewell. dignity retained.",
  "We are concluded.",
  "May you find cleaner streets. quiet now.",
  "Requiescat. quiet now.",
  "Requiescat. dignity retained.",
  "Farewell. quiet now.",
  "Apologies it came to this. dignity retained.",
  "Farewell.",
  "We are concluded. dignity retained.",
  "Requiescat.",
  "Apologies it came to this. quiet now."
};

static const char* S_BUY_GANG[] = {
  "You didn’t get it from me.",
  "Hot goods, cold trail. don’t wave it round.",
  "You didn’t get it from me. spend it fast.",
  "You didn’t get it from me. don’t wave it round.",
  "Good pull.",
  "Hot goods, cold trail.",
  "Hot goods, cold trail. spend it fast.",
  "Good pull. spend it fast.",
  "Paperwork? Never heard of it. don’t wave it round.",
  "Paperwork? Never heard of it.",
  "Paperwork? Never heard of it. spend it fast.",
  "Good pull. don’t wave it round."
};

static const char* S_BUY_NEU[] = {
  "That’ll serve you.",
  "Pleasure doing business.",
  "Solid choice. nice pick.",
  "Solid choice.",
  "That’ll serve you. works on Tuesdays.",
  "Clean sale. works on Tuesdays.",
  "Clean sale.",
  "Pleasure doing business. works on Tuesdays.",
  "Pleasure doing business. nice pick.",
  "Solid choice. works on Tuesdays.",
  "Clean sale. nice pick.",
  "That’ll serve you. nice pick."
};

static const char* S_BUY_POL[] = {
  "Exquisite selection. a pleasure to facilitate.",
  "Curated perfection.",
  "Your taste is impeccable. a pleasure to facilitate.",
  "Exquisite selection.",
  "Curated perfection. it suits you.",
  "Exquisite selection. it suits you.",
  "Your taste is impeccable. it suits you.",
  "Your taste is impeccable.",
  "A discerning acquisition.",
  "A discerning acquisition. a pleasure to facilitate.",
  "A discerning acquisition. it suits you.",
  "Curated perfection. a pleasure to facilitate."
};

static const char* S_SELL_GANG[] = {
  "This never existed.",
  "I like the scuff marks. no receipts.",
  "Cash now, questions never.",
  "This never existed. no receipts.",
  "I like the scuff marks. I know a buyer.",
  "I can fence that.",
  "I can fence that. no receipts.",
  "Cash now, questions never. no receipts.",
  "This never existed. I know a buyer.",
  "I like the scuff marks.",
  "Cash now, questions never. I know a buyer.",
  "I can fence that. I know a buyer."
};

static const char* S_SELL_NEU[] = {
  "Looks workable. neat enough.",
  "I can take it.",
  "Receipt incoming.",
  "Looks workable.",
  "Fair price. neat enough.",
  "Fair price. I’ll move it.",
  "I can take it. I’ll move it.",
  "I can take it. neat enough.",
  "Looks workable. I’ll move it.",
  "Receipt incoming. neat enough.",
  "Fair price.",
  "Receipt incoming. I’ll move it."
};

static const char* S_SELL_POL[] = {
  "Tasteful wear.",
  "Consider it handled. well presented.",
  "Splendid condition. well presented.",
  "Tasteful wear. discretion assured.",
  "Consider it handled.",
  "I’ll ensure it finds a refined owner. well presented.",
  "Tasteful wear. well presented.",
  "Splendid condition. discretion assured.",
  "I’ll ensure it finds a refined owner. discretion assured.",
  "Consider it handled. discretion assured.",
  "Splendid condition.",
  "I’ll ensure it finds a refined owner."
};

static const char* S_DENY_GANG[] = {
  "Not my scene. wouldn’t sell that to enemies.",
  "Can’t move that drek. pawn two alleys east.",
  "Beat it.",
  "Not my scene. pawn two alleys east.",
  "Hard pass. wouldn’t sell that to enemies.",
  "Beat it. wouldn’t sell that to enemies.",
  "Hard pass.",
  "Can’t move that drek. wouldn’t sell that to enemies.",
  "Beat it. pawn two alleys east.",
  "Hard pass. pawn two alleys east.",
  "Can’t move that drek.",
  "Not my scene."
};

static const char* S_DENY_NEU[] = {
  "Not today. try elsewhere.",
  "No deal. come back later.",
  "Not today. come back later.",
  "Doesn’t fit inventory.",
  "No deal. try elsewhere.",
  "Pass.",
  "No deal.",
  "Not today.",
  "Doesn’t fit inventory. come back later.",
  "Doesn’t fit inventory. try elsewhere.",
  "Pass. try elsewhere.",
  "Pass. come back later."
};

static const char* S_DENY_POL[] = {
  "I must decline.",
  "Out of remit. not possible today.",
  "Out of remit.",
  "Out of remit. another avenue.",
  "I’m afraid not. another avenue.",
  "Regrettably unsuitable. not possible today.",
  "I’m afraid not.",
  "I’m afraid not. not possible today.",
  "Regrettably unsuitable.",
  "Regrettably unsuitable. another avenue.",
  "I must decline. not possible today.",
  "I must decline. another avenue."
};

static const char* S_POOR_GANG[] = {
  "No nuyen, no service. window shopping’s free.",
  "I don’t do tabs.",
  "Come back with cred. window shopping’s free.",
  "Come back with cred. ask a loan shark.",
  "Come back with cred.",
  "I don’t do tabs. window shopping’s free.",
  "No nuyen, no service. ask a loan shark.",
  "Luck before commerce. ask a loan shark.",
  "Luck before commerce. window shopping’s free.",
  "I don’t do tabs. ask a loan shark.",
  "No nuyen, no service.",
  "Luck before commerce."
};

static const char* S_POOR_NEU[] = {
  "Budget’s thin. check your balance.",
  "Price is firm. check your balance.",
  "You’ll need funds first. later, maybe.",
  "Not enough. later, maybe.",
  "You’ll need funds first.",
  "Budget’s thin.",
  "You’ll need funds first. check your balance.",
  "Price is firm. later, maybe.",
  "Price is firm.",
  "Not enough. check your balance.",
  "Not enough.",
  "Budget’s thin. later, maybe."
};

static const char* S_POOR_POL[] = {
  "Perhaps another time. save, then savor.",
  "Budget before desire. return when convenient.",
  "Regrettably outside your means. return when convenient.",
  "Perhaps another time. return when convenient.",
  "Budget before desire. save, then savor.",
  "Regrettably outside your means.",
  "Perhaps another time.",
  "I wouldn’t encourage debt. return when convenient.",
  "Regrettably outside your means. save, then savor.",
  "I wouldn’t encourage debt. save, then savor.",
  "Budget before desire.",
  "I wouldn’t encourage debt."
};

static const char* Q_ACCEPT_GANG[] = {
  "Keep comms dark. make it messy or make it fast.",
  "Bring proof, not bodies. make it messy or make it fast.",
  "If it burns, that’s extra.",
  "Don’t die loud.",
  "Don’t die loud. no sermons, just results.",
  "Don’t die loud. make it messy or make it fast.",
  "Keep comms dark.",
  "Keep comms dark. no sermons, just results.",
  "Bring proof, not bodies. no sermons, just results.",
  "If it burns, that’s extra. no sermons, just results.",
  "Bring proof, not bodies.",
  "If it burns, that’s extra. make it messy or make it fast."
};

static const char* Q_ACCEPT_NEU[] = {
  "I’ll log the job. ping me on progress.",
  "You know the drill. ping me on progress.",
  "Got it. Stay sharp.",
  "Got it. Stay sharp. clear the chatter.",
  "Got it. Stay sharp. ping me on progress.",
  "Good hunting. clear the chatter.",
  "Good hunting. ping me on progress.",
  "You know the drill.",
  "I’ll log the job.",
  "Good hunting.",
  "I’ll log the job. clear the chatter.",
  "You know the drill. clear the chatter."
};

static const char* Q_ACCEPT_POL[] = {
  "I look forward to your success. you have my confidence.",
  "Discretion is paramount.",
  "Discretion is paramount. return victorious.",
  "Discretion is paramount. you have my confidence.",
  "I trust your judgment. return victorious.",
  "I look forward to your success. return victorious.",
  "Proceed elegantly. you have my confidence.",
  "I trust your judgment.",
  "I trust your judgment. you have my confidence.",
  "I look forward to your success.",
  "Proceed elegantly.",
  "Proceed elegantly. return victorious."
};

static const char* Q_STEP_GANG[] = {
  "Keep your head low.",
  "Halfway to glory—or the morgue. don’t celebrate yet.",
  "Halfway to glory—or the morgue. stay moving.",
  "Keep your head low. don’t celebrate yet.",
  "Noise down, results up.",
  "Progress. don’t celebrate yet.",
  "Halfway to glory—or the morgue.",
  "Keep your head low. stay moving.",
  "Progress. stay moving.",
  "Noise down, results up. don’t celebrate yet.",
  "Progress.",
  "Noise down, results up. stay moving."
};

static const char* Q_STEP_NEU[] = {
  "Noted. that’ll do.",
  "Noted.",
  "Keep going.",
  "You’re on track. carry on.",
  "Keep going. carry on.",
  "You’re on track. that’ll do.",
  "You’re on track.",
  "Noted. carry on.",
  "Keep going. that’ll do.",
  "Almost there. that’ll do.",
  "Almost there. carry on.",
  "Almost there."
};

static const char* Q_STEP_POL[] = {
  "Performing admirably.",
  "Proceed with care.",
  "Proceed with care. continue as planned.",
  "Splendid progress. continue as planned.",
  "Proceed with care. mind the optics.",
  "Performing admirably. continue as planned.",
  "A fine trajectory. continue as planned.",
  "Performing admirably. mind the optics.",
  "A fine trajectory. mind the optics.",
  "Splendid progress.",
  "Splendid progress. mind the optics.",
  "A fine trajectory."
};

static const char* Q_COMPLETE_GANG[] = {
  "Nice work. wipe your prints.",
  "Payout’s clean. Scram. wipe your prints.",
  "Nice work. we never met.",
  "Ugly and effective—my favorite.",
  "Nice work.",
  "Proof beats promises. wipe your prints.",
  "Payout’s clean. Scram. we never met.",
  "Proof beats promises.",
  "Proof beats promises. we never met.",
  "Payout’s clean. Scram.",
  "Ugly and effective—my favorite. wipe your prints.",
  "Ugly and effective—my favorite. we never met."
};

static const char* Q_COMPLETE_NEU[] = {
  "Job’s a wrap. logs updated.",
  "All squared. logs updated.",
  "Well done.",
  "Well done. logs updated.",
  "Job’s a wrap.",
  "Looks complete. neat finish.",
  "All squared.",
  "Looks complete.",
  "Well done. neat finish.",
  "All squared. neat finish.",
  "Looks complete. logs updated.",
  "Job’s a wrap. neat finish."
};

static const char* Q_COMPLETE_POL[] = {
  "Exemplary work.",
  "Professional to the core. your reputation improves.",
  "Exemplary work. accept my compliments.",
  "Impeccable execution.",
  "Exemplary work. your reputation improves.",
  "Impeccable execution. your reputation improves.",
  "Impeccable execution. accept my compliments.",
  "A pleasure doing business, %name%. accept my compliments.",
  "A pleasure doing business, %name%.",
  "Professional to the core. accept my compliments.",
  "Professional to the core.",
  "A pleasure doing business, %name%. your reputation improves."
};

static const char* R_GET_GANG[] = {
  "This for me. stash it where cameras blush.",
  "I’ll pretend it’s legit. you’re feeding bad habits.",
  "Shiny. Dangerous.",
  "This for me. you’re feeding bad habits.",
  "I’ll pretend it’s legit.",
  "I can make this vanish. stash it where cameras blush.",
  "I can make this vanish. you’re feeding bad habits.",
  "I’ll pretend it’s legit. stash it where cameras blush.",
  "I can make this vanish.",
  "This for me.",
  "Shiny. Dangerous. stash it where cameras blush.",
  "Shiny. Dangerous. you’re feeding bad habits."
};

static const char* R_GET_NEU[] = {
  "Thanks. I’ll log it. noted.",
  "Received.",
  "I’ll stash it.",
  "I’ll stash it. that works.",
  "Appreciated. that works.",
  "Appreciated. noted.",
  "Thanks. I’ll log it.",
  "I’ll stash it. noted.",
  "Received. noted.",
  "Appreciated.",
  "Received. that works.",
  "Thanks. I’ll log it. that works."
};

static const char* R_GET_POL[] = {
  "I’ll see it cared for. gracious as ever.",
  "Consider it accounted for.",
  "Consider it accounted for. gracious as ever.",
  "Much obliged. gracious as ever.",
  "You have my thanks, %name%.",
  "Much obliged. handled discreetly.",
  "Consider it accounted for. handled discreetly.",
  "You have my thanks, %name%. gracious as ever.",
  "I’ll see it cared for.",
  "I’ll see it cared for. handled discreetly.",
  "Much obliged.",
  "You have my thanks, %name%. handled discreetly."
};

struct ListenPack { const char* tokens; const char** g; int gN; const char** n; int nN; const char** p; int pN; };
static ListenPack LISTEN_PACKS[] = {
  { "hello|hi|hey|yo", L_HELLO_GANG, sizeof(L_HELLO_GANG)/sizeof(char*), L_HELLO_NEU, sizeof(L_HELLO_NEU)/sizeof(char*), L_HELLO_POL, sizeof(L_HELLO_POL)/sizeof(char*) },
  { "help|assist|lost", L_HELP_GANG, sizeof(L_HELP_GANG)/sizeof(char*), L_HELP_NEU, sizeof(L_HELP_NEU)/sizeof(char*), L_HELP_POL, sizeof(L_HELP_POL)/sizeof(char*) },
  { "quest|job|work|gig", L_QUEST_GANG, sizeof(L_QUEST_GANG)/sizeof(char*), L_QUEST_NEU, sizeof(L_QUEST_NEU)/sizeof(char*), L_QUEST_POL, sizeof(L_QUEST_POL)/sizeof(char*) },
  { "thanks|thank you|ty", L_THANKS_GANG, sizeof(L_THANKS_GANG)/sizeof(char*), L_THANKS_NEU, sizeof(L_THANKS_NEU)/sizeof(char*), L_THANKS_POL, sizeof(L_THANKS_POL)/sizeof(char*) },
  { 0, 0, 0, 0, 0, 0, 0 }
};

void maybe_greet(struct char_data* ch, struct char_data* mob) {
  if (!ch || !mob || !IS_NPC(mob)) return;
  TalkState& st = g_states[mob];
  if (!can_speak(mob) || nowts() - st.last_greet < 120) return;
  if (!chance(30)) return;
  auto p = get_personality(mob);
  const char* ln = (p == PERS_GANG)    ? pick_nonrepeat(GREET_GANG, sizeof(GREET_GANG)/sizeof(char*), st.last_greet_line)
                  : (p == PERS_NEUTRAL)? pick_nonrepeat(GREET_NEUTRAL, sizeof(GREET_NEUTRAL)/sizeof(char*), st.last_greet_line)
                                       : pick_nonrepeat(GREET_POLITE, sizeof(GREET_POLITE)/sizeof(char*), st.last_greet_line);
  if (!ln) return;
  say_to(mob, ch, expand(ln, ch, mob).c_str());
  st.last_greet = nowts();
  arm_cooldown(mob);
}

void maybe_farewell(struct char_data* ch, struct char_data* mob) {
  if (!ch || !mob || !IS_NPC(mob)) return;
  TalkState& st = g_states[mob];
  if (!can_speak(mob) || nowts() - st.last_farewell < 120) return;
  if (!chance(25)) return;
  auto p = get_personality(mob);
  const char* ln = (p == PERS_GANG)    ? pick_nonrepeat(FAREWELL_GANG, sizeof(FAREWELL_GANG)/sizeof(char*), st.last_farewell_line)
                  : (p == PERS_NEUTRAL)? pick_nonrepeat(FAREWELL_NEUTRAL, sizeof(FAREWELL_NEUTRAL)/sizeof(char*), st.last_farewell_line)
                                       : pick_nonrepeat(FAREWELL_POLITE, sizeof(FAREWELL_POLITE)/sizeof(char*), st.last_farewell_line);
  if (!ln) return;
  say_to(mob, ch, expand(ln, ch, mob).c_str());
  st.last_farewell = nowts();
  arm_cooldown(mob);
}

void maybe_listen(struct char_data* ch, struct char_data* mob, const char* said) {
  if (!ch || !mob || !IS_NPC(mob) || !said) return;
  TalkState& st = g_states[mob];
  if (!can_speak(mob) || nowts() - st.last_listen < 30) return;
  if (!chance(20)) return;
  for (int i = 0; LISTEN_PACKS[i].tokens; ++i) {
    if (token_match(said, LISTEN_PACKS[i].tokens)) {
      auto p = get_personality(mob);
      const char* ln = (p == PERS_GANG)    ? pick_nonrepeat(LISTEN_PACKS[i].g, LISTEN_PACKS[i].gN, st.last_listen_line)
                      : (p == PERS_NEUTRAL)? pick_nonrepeat(LISTEN_PACKS[i].n, LISTEN_PACKS[i].nN, st.last_listen_line)
                                           : pick_nonrepeat(LISTEN_PACKS[i].p, LISTEN_PACKS[i].pN, st.last_listen_line);
      if (!ln) return;
      say_to(mob, ch, expand(ln, ch, mob).c_str());
      st.last_listen = nowts();
      arm_cooldown(mob);
      return;
    }
  }
}

void combat_start(struct char_data* ch, struct char_data* mob) {
  if (!ch || !mob || !IS_NPC(mob)) return;
  TalkState& st = g_states[mob];
  if (st.in_combat) return;
  if (!can_speak(mob) || nowts() - st.last_combat < 60) { st.in_combat = true; return; }
  if (!chance(25)) { st.in_combat = true; return; }
  auto p = get_personality(mob);
  const char* ln = (p == PERS_GANG)    ? pick_nonrepeat(C_START_GANG, sizeof(C_START_GANG)/sizeof(char*), st.last_combat_line)
                  : (p == PERS_NEUTRAL)? pick_nonrepeat(C_START_NEU, sizeof(C_START_NEU)/sizeof(char*), st.last_combat_line)
                                       : pick_nonrepeat(C_START_POL, sizeof(C_START_POL)/sizeof(char*), st.last_combat_line);
  if (ln) say_to(mob, ch, expand(ln, ch, mob).c_str());
  st.last_combat = nowts();
  st.in_combat = true;
  arm_cooldown(mob);
}

void combat_hit(struct char_data* ch, struct char_data* mob) {
  if (!ch || !mob || !IS_NPC(mob)) return;
  TalkState& st = g_states[mob];
  if (!can_speak(mob) || nowts() - st.last_hit < 60) return;
  if (!chance(10)) return;
  auto p = get_personality(mob);
  const char* ln = (p == PERS_GANG)    ? pick_nonrepeat(C_HIT_GANG, sizeof(C_HIT_GANG)/sizeof(char*), st.last_hit_line)
                  : (p == PERS_NEUTRAL)? pick_nonrepeat(C_HIT_NEU, sizeof(C_HIT_NEU)/sizeof(char*), st.last_hit_line)
                                       : pick_nonrepeat(C_HIT_POL, sizeof(C_HIT_POL)/sizeof(char*), st.last_hit_line);
  if (!ln) return;
  say_to(mob, ch, expand(ln, ch, mob).c_str());
  st.last_hit = nowts();
  arm_cooldown(mob);
}

void combat_death(struct char_data* ch, struct char_data* mob) {
  if (!ch || !mob || !IS_NPC(mob)) return;
  TalkState& st = g_states[mob];
  if (!can_speak(mob) || nowts() - st.last_death < 30) return;
  if (!chance(50)) return;
  auto p = get_personality(mob);
  const char* ln = (p == PERS_GANG)    ? pick_nonrepeat(C_DEATH_GANG, sizeof(C_DEATH_GANG)/sizeof(char*), st.last_death_line)
                  : (p == PERS_NEUTRAL)? pick_nonrepeat(C_DEATH_NEU, sizeof(C_DEATH_NEU)/sizeof(char*), st.last_death_line)
                                       : pick_nonrepeat(C_DEATH_POL, sizeof(C_DEATH_POL)/sizeof(char*), st.last_death_line);
  if (!ln) return;
  say_to(mob, ch, expand(ln, ch, mob).c_str());
  st.last_death = nowts();
  st.in_combat = false;
  arm_cooldown(mob);
}

void shop_event(struct char_data* ch, struct char_data* mob, int type) {
  if (!ch || !mob || !IS_NPC(mob)) return;
  TalkState& st = g_states[mob];
  if (!can_speak(mob) || nowts() - st.last_shop < 30) return;
  if (!chance(40)) return;
  auto p = get_personality(mob);
  const char* ln = nullptr;
  switch (type) {
    case 1:
      ln = (p == PERS_GANG)    ? pick_nonrepeat(S_BUY_GANG, sizeof(S_BUY_GANG)/sizeof(char*), st.last_shop_line)
         : (p == PERS_NEUTRAL)? pick_nonrepeat(S_BUY_NEU, sizeof(S_BUY_NEU)/sizeof(char*), st.last_shop_line)
                              : pick_nonrepeat(S_BUY_POL, sizeof(S_BUY_POL)/sizeof(char*), st.last_shop_line);
      break;
    case 2:
      ln = (p == PERS_GANG)    ? pick_nonrepeat(S_SELL_GANG, sizeof(S_SELL_GANG)/sizeof(char*), st.last_shop_line)
         : (p == PERS_NEUTRAL)? pick_nonrepeat(S_SELL_NEU, sizeof(S_SELL_NEU)/sizeof(char*), st.last_shop_line)
                              : pick_nonrepeat(S_SELL_POL, sizeof(S_SELL_POL)/sizeof(char*), st.last_shop_line);
      break;
    case 3:
      ln = (p == PERS_GANG)    ? pick_nonrepeat(S_DENY_GANG, sizeof(S_DENY_GANG)/sizeof(char*), st.last_shop_line)
         : (p == PERS_NEUTRAL)? pick_nonrepeat(S_DENY_NEU, sizeof(S_DENY_NEU)/sizeof(char*), st.last_shop_line)
                              : pick_nonrepeat(S_DENY_POL, sizeof(S_DENY_POL)/sizeof(char*), st.last_shop_line);
      break;
    case 4:
      ln = (p == PERS_GANG)    ? pick_nonrepeat(S_POOR_GANG, sizeof(S_POOR_GANG)/sizeof(char*), st.last_shop_line)
         : (p == PERS_NEUTRAL)? pick_nonrepeat(S_POOR_NEU, sizeof(S_POOR_NEU)/sizeof(char*), st.last_shop_line)
                              : pick_nonrepeat(S_POOR_POL, sizeof(S_POOR_POL)/sizeof(char*), st.last_shop_line);
      break;
  }
  if (!ln) return;
  say_to(mob, ch, expand(ln, ch, mob).c_str());
  st.last_shop = nowts();
  arm_cooldown(mob);
}

void quest_event(struct char_data* ch, struct char_data* mob, int type) {
  if (!ch || !mob || !IS_NPC(mob)) return;
  TalkState& st = g_states[mob];
  if (!can_speak(mob) || nowts() - st.last_quest < 20) return;
  if (!chance(60)) return;
  auto p = get_personality(mob);
  const char* ln = nullptr;
  switch (type) {
    case 1:
      ln = (p == PERS_GANG)    ? pick_nonrepeat(Q_ACCEPT_GANG, sizeof(Q_ACCEPT_GANG)/sizeof(char*), st.last_quest_line)
         : (p == PERS_NEUTRAL)? pick_nonrepeat(Q_ACCEPT_NEU, sizeof(Q_ACCEPT_NEU)/sizeof(char*), st.last_quest_line)
                              : pick_nonrepeat(Q_ACCEPT_POL, sizeof(Q_ACCEPT_POL)/sizeof(char*), st.last_quest_line);
      break;
    case 2:
      ln = (p == PERS_GANG)    ? pick_nonrepeat(Q_STEP_GANG, sizeof(Q_STEP_GANG)/sizeof(char*), st.last_quest_line)
         : (p == PERS_NEUTRAL)? pick_nonrepeat(Q_STEP_NEU, sizeof(Q_STEP_NEU)/sizeof(char*), st.last_quest_line)
                              : pick_nonrepeat(Q_STEP_POL, sizeof(Q_STEP_POL)/sizeof(char*), st.last_quest_line);
      break;
    case 3:
      ln = (p == PERS_GANG)    ? pick_nonrepeat(Q_COMPLETE_GANG, sizeof(Q_COMPLETE_GANG)/sizeof(char*), st.last_quest_line)
         : (p == PERS_NEUTRAL)? pick_nonrepeat(Q_COMPLETE_NEU, sizeof(Q_COMPLETE_NEU)/sizeof(char*), st.last_quest_line)
                              : pick_nonrepeat(Q_COMPLETE_POL, sizeof(Q_COMPLETE_POL)/sizeof(char*), st.last_quest_line);
      break;
  }
  if (!ln) return;
  say_to(mob, ch, expand(ln, ch, mob).c_str());
  st.last_quest = nowts();
  arm_cooldown(mob);
}

void receive_item(struct char_data* ch, struct char_data* mob, struct obj_data* obj) {
  if (!ch || !mob || !IS_NPC(mob)) return;
  TalkState& st = g_states[mob];
  if (!can_speak(mob) || nowts() - st.last_receive < 60) return;
  if (!chance(50)) return;
  auto p = get_personality(mob);
  const char* ln = (p == PERS_GANG)    ? pick_nonrepeat(R_GET_GANG, sizeof(R_GET_GANG)/sizeof(char*), st.last_recv_line)
                  : (p == PERS_NEUTRAL)? pick_nonrepeat(R_GET_NEU, sizeof(R_GET_NEU)/sizeof(char*), st.last_recv_line)
                                       : pick_nonrepeat(R_GET_POL, sizeof(R_GET_POL)/sizeof(char*), st.last_recv_line);
  if (!ln) return;
  say_to(mob, ch, expand(ln, ch, mob).c_str());
  st.last_receive = nowts();
  arm_cooldown(mob);
}

} // namespace NPCVoice
