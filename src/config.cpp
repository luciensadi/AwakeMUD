#include "structs.h"
#include "awake.h"

int max_exp_gain = 250; /* max gainable per kill */
int max_npc_corpse_time = 3;
int max_pc_corpse_time = 10;
const char *OK = "Okay.\r\n";
const char *TOOBUSY = "You're too busy.\r\n";
const char *YOU_NEED_OLC_FOR_THAT = "Please SET SELF OLC ON (or ask a higher-level staff member to do it).\r\n";
const char *DEFAULT_POOFIN_STRING = "$n appears out of nowhere.";
const char *DEFAULT_POOFOUT_STRING = "$n disappears in a flash of light.";
const char *KILLER_FLAG_MESSAGE = "Your actions have earned you the KILLER flag, which makes you fair game to all player characters.\r\n";
const char *CANNOT_GO_THAT_WAY = "You cannot go that way...\r\n";
const char *CHARACTER_DELETED_NAME_FOR_SQL = "deleted";
long newbie_start_room = 60500;
long mortal_start_room = 30700;
//long mortal_start_room = 35500;
long immort_start_room = 10000;
long frozen_start_room = 10045;
vnum_t donation_room_1 = 60570;
vnum_t donation_room_2 = 60571;
vnum_t donation_room_3 = 60572;
int DFLT_PORT = 4000;
const char *DFLT_DIR = "lib";
int MAX_PLAYERS = 300;
int max_filesize = 50000;
int max_bad_pws = 3;
int nameserver_is_slow = FALSE;
int ELEVATOR_SHAFT_FALL_RATING = 6;
int max_things_you_can_purchase_at_once = 50;

bool DISPLAY_HELPFUL_STRINGS_FOR_MOB_FUNCS = TRUE;

const char *MENU =
  "\r\n"
  "^YWelcome ^Cto ^BAwakened Worlds^n!\r\n"
  "^R0^n) Exit from ^BAwakened Worlds^n.\r\n"
  "^G1^n) Enter the game.\r\n"
  "^G2^n) Read the background story.\r\n"
  "^G3^n) Change password.\r\n"
  "^L4^n) Delete this character.\r\n"
  "\r\n"
  "   Make your choice: ";

const char *QMENU =
  "\r\n"
  "Current options:\r\n"
  "^R0^n) Exit from ^BAwakened Worlds^n.\r\n"
  "^G1^n) Delete this character.\r\n"
  "\r\n"
  "   Make your choice: ";

const char *GREETINGS =
  "\r\n"
  "Administration Email: luciensadi@gmail.com\r\n"
  "Credit to Jeremy Elson, Hans Henrik Staerfeldt, Katja Nyboe, Tom Madsen,\r\n"
  "Michael Seifert, and Sebastian Hammer for their work on predecessor games.\r\n"
  "Forked from AwakeMUD 0.8.20 BETA, by Flynn, Fastjack, Rift, Washu, and Che.\r\n"
  "This is AwakeMUD Community Edition 0.9.0, by Lucien, Finster, and the community.\r\n"
  "\r\n"
  "_____   ^C.^n                    ^WA^n            ^C.^n              ^W.^n   ^W.^n       ^C.^n\r\n"
  "o o o\\            ^W.^n        ^W_/_\\_^n                                  |\\\r\n"
  "------\\\\      ^c.^n         ^W__//...\\\\__^n                ^W.^n              ||\\   ^C.^n\r\n"
  "__ A ^y.^n |\\           ^W.^n  ^r<^n-^r-^n-^r-^n-^r-^n-^r-^n-^r-^n-^r>^n     ^W.^n                  ^C.^n     ||||\r\n"
  "HH|\\^y.^n .|||                ^W\\\\\\|///^n                 ___^R|^n_           ||||\r\n"
  "||| | ^Y.^n \\\\\\     A    ^C.^n      ^W|^n.^W|^n                  /|  .|    ^W.^n      /||\\\r\n"
  "  | | ^y.^n  |||   / \\          ^W|^n.^W|^n     .           | | ..|          /.||.\\\r\n"
  "^y..^n| | ^y. .^n \\\\\\ ||^y*^n*|         ^W|^n.^W|^n   _A_     ___   | | ..|         || |\\ .|\r\n"
  "^y..^n| | , ,  |||||^y**^n|         ^W|^n.^W|^n  /| |   /|   |  |.| ..|         || |*|*|\r\n"
  "^Y.^W.^n|.| ^W.^n . ^y.^n \\\\\\|^Y*^y*^n|.  ____  ^W|^n.^W|^n | | |  | |^y*^Y*^y*^n|  |.| ..|  _____  || |*|*|\r\n"
  ".^y.^n|.| . ^y.^n .  |||^y**^n| /|.. .| ^W|^n.^W|^n |^y*^n|^y*^n|  | |^y*^n  | ___| ..|/|  .  | ||.|*|\\|\\\r\n"
  "-----------,. \\\\\\^y*^n|| |.. .|^W//|\\\\^n|*|*_____| ^y**^n||| ||  .| | ..  |/|| |*| |\\\\\r\n"
  "AwakeMUD CE \\  ||||| |..  ^W// A \\\\^n*/| . ..| ^y*^n ||| || ..| |  .  ||||,|*| | \\\r\n"
  " By Lucien  |\\. \\\\\\| |.. ^W// /|\\ \\\\^n | . ..|^y**^n ||| || ..| | . . ||||.|*| |\\\\\r\n"
  "  and the    \\\\  ||| |..^W|| | | | ||^n| . ..| ^y*^n ||| ||  .| | ..  ||||.|*| ||||\r\n"
  "CE Community  || ||| |, ^W||.| | | ||^n| . ..| ^y*^n ||| || ..| | . ..||||.|*| ||||\r\n"
  "---------------------------------------------------------------------------\r\n"
  "\r\n"
  "What's your handle, chummer? ";

const char *WELC_MESSG =
  "\r\n"
  "Welcome to the future, 2064, the Sixth World to some, an Awakening to all.\r\n"
  "\r\n\r\n";

const char *START_MESSG =
  "Welcome to the future, 2064, where humankind has entered what the Mayans would\r\n"
  "call the Sixth World. You are entering a world of rampant corruption and twisted\r\n"
  "mechanisms of power, a place where corporations overrule nations and life is cheap.\r\n"
  "You have joined this world as a Shadowrunner, a person who works clandestine jobs\r\n"
  "to make ends meet. You'll need to keep your wits about you if you want to survive.\r\n"
  "\r\n"
  "If you are using a screenreader or related assistive technology, please type\r\n"
  "TOGGLE SCREENREADER to enable our assistive features.\r\n"
  "\r\n"
  "You are now in the tutorial area. If you ever need assistance, you can reach out\r\n"
  "by typing NEWBIE <your question here>. Example: 'newbie Hello, how do I...'\r\n"
  "\r\n";
