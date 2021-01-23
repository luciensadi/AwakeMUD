#ifndef _config_h
#define _config_h

extern int max_exp_gain;
extern int max_pc_corpse_time;
extern int max_npc_corpse_time;
extern int DFLT_PORT;
extern char *DFLT_DIR;
extern int MAX_PLAYERS;
extern int max_filesize;
extern int max_bad_pws;
extern int nameserver_is_slow;
extern const char *OK;
extern const char *TOOBUSY;
extern const char *MENU;
extern const char *QMENU;
extern const char *GREETINGS;
extern const char *WELC_MESSG;
extern const char *START_MESSG;
extern const char *YOU_NEED_OLC_FOR_THAT;
extern const char *DEFAULT_POOFIN_STRING;
extern const char *DEFAULT_POOFOUT_STRING;
extern const char *CANNOT_GO_THAT_WAY;
extern const char *CHARACTER_DELETED_NAME_FOR_SQL;

// What karma / nuyen multipliers do you want your game to have? This effects grind length, higher is faster.
#define KARMA_GAIN_MULTIPLIER 2.0
#define NUYEN_GAIN_MULTIPLIER 2.0

// How well should markets regenerate over time? (currently every 2 mins)
#define MAX_PAYDATA_MARKET_INCREASE_PER_TICK 250
#define MIN_PAYDATA_MARKET_INCREASE_PER_TICK -5

// What should the paydata markets cap out at?
#define HOST_SECURITY_BLUE_MARKET_MAXIMUM   2000
#define HOST_SECURITY_GREEN_MARKET_MAXIMUM  4000
#define HOST_SECURITY_ORANGE_MARKET_MAXIMUM 6000
#define HOST_SECURITY_RED_MARKET_MAXIMUM    8000
#define HOST_SECURITY_BLACK_MARKET_MAXIMUM  10000

#define HOST_SECURITY_BLUE_MARKET_MINIMUM   500
#define HOST_SECURITY_GREEN_MARKET_MINIMUM  1000
#define HOST_SECURITY_ORANGE_MARKET_MINIMUM 1500
#define HOST_SECURITY_RED_MARKET_MINIMUM    2000
#define HOST_SECURITY_BLACK_MARKET_MINIMUM  2500

// What maximum amount of karma per action do you want PCs < 100 TKE to have?
#define MAX_NEWCHAR_GAIN  50

// What maximum amount of karma per action do you want PCs < 500 TKE to have?
#define MAX_MIDCHAR_GAIN  100

// What maximum amount of karma per action do you want PCs >= 500 TKE to have?
#define MAX_OLDCHAR_GAIN  MAX(100, GET_TKE(ch) / 4)

// What do you want the maximum skill levels to be? Reference values in awake.h.
#define MAX_SKILL_LEVEL_FOR_MORTS  LEARNED_LEVEL
#define MAX_SKILL_LEVEL_FOR_IMMS   100

// What do you want the newbie karma threshold to be? Above this, you lose the newbie flag.
#define NEWBIE_KARMA_THRESHOLD  50

// How many syspoints should someone spend to restring an item?
#define SYSP_RESTRING_COST  2

// How long should the MUD wait for recovery before killing itself? Note that it
// considers itself to be stuck during copyover too, so if you have a large world,
// you should increase this value.
#define SECONDS_TO_WAIT_FOR_HUNG_MUD_TO_RECOVER_BEFORE_KILLING_IT 30

// What is the maximum cab fare in nuyen?
#define MAX_CAB_FARE  250

// What should the level of new characters' starting language be?
#define STARTING_LANGUAGE_SKILL_LEVEL  10

// How many restring points should newbies get to use in chargen?
#define STARTING_RESTRING_POINTS  5

// What are the default settings of a room if not specified?
#define DEFAULT_DIMENSIONS_X        20
#define DEFAULT_DIMENSIONS_Y        20
#define DEFAULT_DIMENSIONS_Z        2.5
#define DEFAULT_EXIT_BARRIER_RATING 4
#define DEFAULT_EXIT_MATERIAL       5
#define DEFAULT_SECTOR_TYPE         SPIRIT_CITY

// Bearing in mind that morts can't interact with an exit if they can't see it, what is the maximum hidden rating for an exit?
#define MAX_EXIT_HIDDEN_RATING      10

// Thresholds for warning builders about anomalies.
#define ANOMALOUS_HIDDEN_RATING_THRESHOLD 10
#define ANOMALOUS_TOTAL_STATS_THRESHOLD 60
#define ANOMALOUS_SKILL_THRESHOLD 10

// How often should congregation bonus points be accrued?
#define SECONDS_BETWEEN_CONGREGATION_POOL_GAINS 60
// What is the maximum congregation bonus that can be accrued? If you change this, also take a look at score in act.informative.cpp and make sure the spacing matches up.
#define MAX_CONGREGATION_BONUS 500
#define CONGREGATION_MULTIPLIER 1.5
// Note that these values are in hundredths of karma.
#define CONGREGATION_MAX_KARMA_GAIN_PER_ACTION 25
#define CONGREGATION_MIN_KARMA_GAIN_PER_ACTION 10
// The min value must be less than the max value. It ensures people don't feel like they're wasting their karma mult if the accidentally kill a 0.01-karma mob.

// How infrequently do you want your trideo units to display something?
#define TRIDEO_TICK_DELAY 3

// How many rooms can gridguide go at max speed? Too high, and vehicles will teleport.
#define MAX_GRIDGUIDE_ROOMS_PER_PULSE 10

// How often do NPCs press elevator buttons? 1:x ratio, where X is the number you put here.
#define ELEVATOR_BUTTON_PRESS_CHANCE 20

// Wizlock message.
#define WIZLOCK_MSG "Sorry, new characters can't be created at the moment. In the meantime, join our Discord at https://discord.gg/q5VCMkv!"

// Discord server URL. Comment out if you don't have one.
#define DISCORD_SERVER_URL "https://discord.gg/q5VCMkv"

#endif
