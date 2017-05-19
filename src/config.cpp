#include "structs.h"
#include "awake.h"

int max_exp_gain = 250; /* max gainable per kill */
int max_npc_corpse_time = 3;
int max_pc_corpse_time = 10;
char *OK = "Okay.\r\n";
char *NOPERSON = "You don't see that person here.\r\n";
char *TOOBUSY = "You're too busy.\r\n";
char *NOOBJECT = "You don't see that here.\r\n";
long newbie_start_room = 60500;
//long mortal_start_room = 30700;
long mortal_start_room = 1;
long immort_start_room = 1000;
long frozen_start_room = 1050;
vnum_t donation_room_1 = 60570;
vnum_t donation_room_2 = 60571;
vnum_t donation_room_3 = 60572;
int DFLT_PORT = 4000;
char *DFLT_DIR = "lib";
int MAX_PLAYERS = 300;
int max_filesize = 50000;
int max_bad_pws = 3;
int nameserver_is_slow = FALSE;

char *MENU =
  "\r\n"
  "^YWelcome ^Cto ^BAwakened Worlds^n!\r\n"
  "^R0^n) Exit from ^BAwakened Worlds^n.\r\n"
  "^G1^n) Enter the game.\r\n"
  "^G2^n) Read the background story.\r\n"
  "^G3^n) Change password.\r\n"
  "^L4^n) Delete this character.\r\n"
  "\r\n"
  "   Make your choice: ";

char *QMENU =
  "\r\n"
  "Current options:\r\n"
  "^R0^n) Exit from ^BAwakened Worlds^n.\r\n"
  "^G1^n) Delete this character.\r\n"
  "\r\n"
  "   Make your choice: ";

char *GREETINGS =
  "\r\n"
  "Administration Email: luciensadi@gmail.com\r\n"
  "The following mud is based on CircleMUD 3.0 by Jeremy Elson.  It is a\r\n"
  "derivative of DikuMUD (GAMMA 0.0) by Hans Henrik Staerfeldt, Katja Nyboe,\r\n"
  "Tom Madsen, Michael Seifert, and Sebastian Hammer.\r\n"
  "AwakeMUD Code Level 0.8.19 BETA, by Flynn, Fastjack, Rift, Washu, and Che.\r\n"
  "Modifications by Lucien Sadi.\r\n"
  "\r\n"
  "_____   .                    A            .              .   .       .\r\n"
  "o o o\\            .        _/_\\_                                  |\\\r\n"
  "------\\\\      .         __//...\\\\__                .              ||\\   .\r\n"
  "__ A . |\\           .  <----------->     .                  .     ||||\r\n"
  "HH|\\. .|||                \\\\\\|///                 ___|_           ||||\r\n"
  "||| | . \\\\\\     A    .      |.|                  /|  .|    .      /||\\\r\n"
  "  | | .  |||   / \\          |.|     .           | | ..|          /.||.\\\r\n"
  "..| | . . \\\\\\ ||**|         |.|   _A_     ___   | | ..|         || |\\ .|\r\n"
  "..| | , ,  |||||**|         |.|  /| |   /|   |  |.| ..|         || |*|*|\r\n"
  "..|.| . . . \\\\\\|**|.  ____  |.| | | |  | |***|  |.| ..|  _____  || |*|*|\r\n"
  "..|.| . . .  |||**| /|.. .| |.| |*|*|  | |*  | ___| ..|/|  .  | ||.|*|\\|\\\r\n"
  "_________ . . \\\\\\*|| |.. .|//|\\\\|*|*_____| **||| ||  .| | ..  |/|| |*| |\\\\\r\n"
  "AwakeMUD  \\ .  ||||| |..  // A \\\\*/| . ..| * ||| || ..| |  .  ||||,|*| | \\\r\n"
  "By:        |\\ . \\\\\\| |.. // /|\\ \\\\ | . ..|** ||| || ..| | . . ||||.|*| |\\\\\r\n"
  "Rift, Pook  \\\\.  ||| |..|| | | | ||| . ..| * ||| ||  .| | ..  ||||.|*| ||||\r\n"
  "& Dunkelzahn ||  ||| |, ||.| | | ||| . ..| * ||| || ..| | . ..||||.|*| ||||\r\n"
  "---------------------------------------------------------------------------\r\n"
  "\r\n"
  "Slot me some identification, chummer: ";

char *WELC_MESSG =
  "\r\n"
  "Welcome to the future, 2064, the Sixth World to some, an Awakening to all.\r\n"
  "\r\n\r\n";

char *START_MESSG =
  "Welcome to the future, 2064, where mankind has entered what the Mayans would\r\n"
  "call the Sixth World.  New races, magic, and technology all clash in what we\r\n"
  "call Awakened Worlds.  If you have never experienced this world before, typing\r\n"
  "HELP NEWBIE will help you understand what exactly is going on here.\r\n\r\n";
