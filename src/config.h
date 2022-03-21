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
#define KARMA_GAIN_MULTIPLIER                                  2.0
#define NUYEN_GAIN_MULTIPLIER                                  2.0

// How well should markets regenerate over time? (currently every 2 mins)
#define MAX_PAYDATA_MARKET_INCREASE_PER_TICK                   250
#define MIN_PAYDATA_MARKET_INCREASE_PER_TICK                   -5

// What should the paydata markets cap out at?
#define HOST_SECURITY_BLUE_MARKET_MAXIMUM                      2000
#define HOST_SECURITY_GREEN_MARKET_MAXIMUM                     4000
#define HOST_SECURITY_ORANGE_MARKET_MAXIMUM                    6000
#define HOST_SECURITY_RED_MARKET_MAXIMUM                       8000
#define HOST_SECURITY_BLACK_MARKET_MAXIMUM                     10000

#define HOST_SECURITY_BLUE_MARKET_MINIMUM                      500
#define HOST_SECURITY_GREEN_MARKET_MINIMUM                     1000
#define HOST_SECURITY_ORANGE_MARKET_MINIMUM                    1500
#define HOST_SECURITY_RED_MARKET_MINIMUM                       2000
#define HOST_SECURITY_BLACK_MARKET_MINIMUM                     2500

#define MAX_PAYDATA_QTY_BLUE                                   4
#define MAX_PAYDATA_QTY_GREEN                                  8
#define MAX_PAYDATA_QTY_ORANGE                                 10
#define MAX_PAYDATA_QTY_RED_BLACK                              12

// What maximum amount of karma per action do you want PCs < 100 TKE to have?
#define MAX_NEWCHAR_GAIN                                       50

// What maximum amount of karma per action do you want PCs < 500 TKE to have?
#define MAX_MIDCHAR_GAIN                                       100

// What maximum amount of karma per action do you want PCs >= 500 TKE to have?
#define MAX_OLDCHAR_GAIN                                       MAX(100, GET_TKE(ch) / 4)

// What do you want the maximum skill levels to be? Reference values in awake.h.
#define MAX_SKILL_LEVEL_FOR_MORTS                              LEARNED_LEVEL
#define MAX_SKILL_LEVEL_FOR_IMMS                               100

// What do you want the newbie karma threshold to be? Above this, you lose the newbie flag.
#define NEWBIE_KARMA_THRESHOLD                                 50

// How many syspoints do the various options cost?
#define SYSP_RESTRING_COST                                     2
#define SYSP_NODELETE_COST                                     100

// How long should the MUD wait for recovery before killing itself? Note that it
// considers itself to be stuck during copyover too, so if you have a large world,
// you should increase this value.
#define SECONDS_TO_WAIT_FOR_HUNG_MUD_TO_RECOVER_BEFORE_KILLING_IT 30

// What is the maximum cab fare in nuyen?
#define MAX_CAB_FARE                                           250

// What should the level of new characters' starting language be?
#define STARTING_LANGUAGE_SKILL_LEVEL                          10

// How many restring points should newbies get to use in chargen?
#define STARTING_RESTRING_POINTS                               5

// What are the default settings of a room if not specified?
#define DEFAULT_DIMENSIONS_X                                   20
#define DEFAULT_DIMENSIONS_Y                                   20
#define DEFAULT_DIMENSIONS_Z                                   2.5
#define DEFAULT_EXIT_BARRIER_RATING                            4
#define DEFAULT_EXIT_MATERIAL                                  5
#define DEFAULT_SECTOR_TYPE                                    SPIRIT_CITY

// Bearing in mind that morts can't interact with an exit if they can't see it, what is the maximum hidden rating for an exit?
#define MAX_EXIT_HIDDEN_RATING                                 10

// Thresholds for warning builders about anomalies.
#define ANOMALOUS_HIDDEN_RATING_THRESHOLD                      10
#define ANOMALOUS_TOTAL_STATS_THRESHOLD                        60
#define ANOMALOUS_SKILL_THRESHOLD                              10

// How often should congregation bonus points be accrued?
#define SECONDS_BETWEEN_CONGREGATION_POOL_GAINS                60
// What is the maximum congregation bonus that can be accrued? If you change this, also take a look at score in act.informative.cpp and make sure the spacing matches up.
#define MAX_CONGREGATION_BONUS                                 500
#define CONGREGATION_MULTIPLIER                                1.5
// Note that these values are in hundredths of karma.
#define CONGREGATION_MAX_KARMA_GAIN_PER_ACTION                 25
#define CONGREGATION_MIN_KARMA_GAIN_PER_ACTION                 10
// The min value must be less than the max value. It ensures people don't feel like they're wasting their karma mult if the accidentally kill a 0.01-karma mob.

// How infrequently do you want your trideo units to display something?
#define TRIDEO_TICK_DELAY                                      3

// How many rooms can gridguide go at max speed? Too high, and vehicles will teleport.
#define MAX_GRIDGUIDE_ROOMS_PER_PULSE                          10

// How often do NPCs press elevator buttons? 1:x ratio, where X is the number you put here.
#define ELEVATOR_BUTTON_PRESS_CHANCE                           20

// Wizlock message.
#define WIZLOCK_MSG "Sorry, the game is currently locked. While you wait for it to open, feel free to join our Discord at https://discord.gg/q5VCMkv!"

// Discord server URL. Comment out if you don't have one.
#define DISCORD_SERVER_URL "https://discord.gg/q5VCMkv"

// Credsticks should generate every 1:X kills, and have X * kill's credits. Setting this too high will cause things like wageslaves having silver credsticks, so be cautious.
#define CREDSTICK_RARITY_FACTOR                                10

// At what point will the Marksman quest trigger?
#define MARKSMAN_QUEST_SHOTS_FIRED_REQUIREMENT                 2000

// How likely are you to lose stats on death? 1/X where X is this value.
#define DEATH_PENALTY_CHANCE                                   25
// How much nuyen do you lose on death? 1/X where X is this value.
#define DEATH_NUYEN_LOSS_DIVISOR                               4

// Configs for the idle nuyen reward, in nuyen and minutes respectively.
#define IDLE_NUYEN_REWARD_AMOUNT                               1000
#define IDLE_NUYEN_REWARD_THRESHOLD_IN_MINUTES                 10
#define IDLE_NUYEN_MINUTES_BETWEEN_AWARDS                      60

// What is the multiplier at which a cyberdoc will buy 'ware from you?
#define CYBERDOC_MAXIMUM_SELL_TO_SHOPKEEP_MULTIPLIER           0.30

// How much nuyen do you have to fork over to get an NPC to install 'ware for you?
// Docs charge 1/factor of the price, up to the maximum configured.
#define CYBERWARE_INSTALLATION_COST_FACTOR                     10
#define CYBERWARE_INSTALLATION_COST_MAXIMUM                    1000000
#define CYBERDOC_NO_INJURY_DIE_REQUIREMENT                     5

// You have to put down 33% of the cost of the item to order it, and orders stick around for X IRL days.
#define PREORDER_COST_DIVISOR                                  3
#define PREORDERS_ARE_GOOD_FOR_X_DAYS                          7
#define PREORDER_RESTOCKING_FEE_DIVISOR                        10

// How many minutes must have passed since the creation of a piece of mail before it is auto-deleted?
#define MAIL_EXPIRATION_TICKS                                  (60 * 24 * 14) /* 14 days */

// Configuration for the "where's my car" feature.
// How many nuyen do NPCs charge per room between searcher and vehicle?
#define WHERES_MY_CAR_NUYEN_PER_ROOM                           10
#define WHERES_MY_CAR_MINIMUM_NUYEN_CHARGE                     (WHERES_MY_CAR_NUYEN_PER_ROOM * 50)

// How much nuyen does it cost to paint (restring) your vehicle?
#define PAINTER_COST                                           20000

// How much karma does it cost to learn close combat, and at what skill threshold do NPCs figure out how to use it?
#define KARMA_COST_FOR_CLOSECOMBAT                             200
#define NPC_SKILL_THRESHOLD_FOR_FREE_SWITCHING_OF_CLOSECOMBAT  8

#define KARMA_COST_FOR_KIPUP                                   200

// What is the standard batch size for ammo creation? Rules say 10, but you say...
#define AMMOBUILD_BATCH_SIZE                                   10

// What is the minimum cost to remove a gas vent from a weapon?
#define MINIMUM_GAS_VENT_REMOVAL_COST                          500

// What is the minimum number of connections per 10 seconds that you want to engage rudimentary protections at?
#define DOS_SLOW_NS_THRESHOLD                                  (8 * 10)
// What is the "" that you want to deny new connections at?
#define DOS_DENIAL_THRESHOLD                                   (15 * 10)
// What is the number of seconds that you'd consider to be too long to pause the game for resolving a player's domain name?
#define THRESHOLD_IN_SECONDS_FOR_SLOWNS_AUTOMATIC_ACTIVATION   1

// How long do we put characters in a wait state when performing various magical activities?
#define SPELL_WAIT_STATE_TIME                                  (0.2  RL_SEC)
#define OFFENSIVE_SPELL_WAIT_STATE_TIME                        (0.2  RL_SEC)
#define FAILED_SPELL_LEARNING_WAIT_STATE                       (0.25 RL_SEC)
#define INITIATION_CAP                                         50

#define MAX_MOB_COMBAT_MAGIC_FORCE                             10
#define MIN_MOB_COMBAT_MAGIC_FORCE                             4

#define MAX_MOB_COMBAT_MAGIC_WOUND                             SERIOUS
#define MIN_MOB_COMBAT_MAGIC_WOUND                             MODERATE

// How many sunrises (currently spans of 48 minutes) are spirits good for?
#define NUMBER_OF_IG_DAYS_FOR_SPIRIT_TO_LAST                   4

// How long is the decking command wait state?
#define DECKING_WAIT_STATE_TIME                                (0.2 RL_SEC)

// How long is the ram command wait state?
#define RAM_WAIT_STATE_TIME                                    (0.4 RL_SEC)

// Spam prevention: What's the maximum amount of graffiti in a room?
#define MAXIMUM_GRAFFITI_IN_ROOM                               3

// What's the maximum difficulty allowable for a closing-the-distance check?
// Repo default is 10, which is a 20% chance of success at 10 dice. Most high-level PCs have 18+, giving ~45% success.
#define MAXIMUM_TN_FOR_CLOSING_CHECK                           10
#define MINIMUM_TN_FOR_CLOSING_CHECK                           4

// At what point do we consider someone to be too idle in a scene to get socialization bonuses?
#define LAST_SOCIAL_ACTION_REQUIREMENT_FOR_CONGREGATION_BONUS                20
#define SOCIAL_ACTION_GRACE_PERIOD_GRANTED_BY_SPEECH                         5

// Don't @ me about how long this name is, this is what we call self-documenting code.
#define NUMBER_OF_TKE_POINTS_PER_REAL_DAY_OF_EXTRA_IDLE_DELETE_GRACE_PERIOD  10

// If you don't think the same-room +6 TN penalty for using a sniper rifle is appropriate, adjust here.
#define SAME_ROOM_SNIPER_RIFLE_PENALTY                         6

/////////////// OLC permissions configuration /////////////////////////
#define LVL_FOR_SETTING_ZONE_EDITOR_ID_NUMBERS                 LVL_VICEPRES
#define LVL_FOR_SETTING_ZONE_CONNECTED_STATUS                  LVL_FIXER

#endif
