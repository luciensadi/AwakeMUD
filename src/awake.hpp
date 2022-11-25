/* ****************************************
* file: awake.h -- AwakeMUD Header File   *
* A centralized definition file for all   *
* the major defines, etc...               *
* (c)2001 the AwakeMUD Consortium         *
* Code migrated from other header files   *
* (such as spells.h) (c) 1993, 94 by the  *
* trustees of the John Hopkins University *
* and (c) 1990-1991 The DikuMUD group     *
**************************************** */

#ifndef _awake_h_
#define _awake_h_

#include <time.h>
#include <iostream>

#if (!defined(WIN32) || defined(__CYGWIN__)) && !defined(osx)
// using namespace std;
#endif

#define NUM_RESERVED_DESCS      8
#define NUM_VALUES    12
# ifdef DEBUG
# define _STLP_DEBUG 1
# else
# undef _STLP_DEBUG
# endif

/* pertaining to vnums/rnums */

#define REAL 0
#define VIRTUAL 1

/* gender */

#define SEX_NEUTRAL   0
#define SEX_MALE      1
#define SEX_FEMALE    2

/* positions */

#define POS_DEAD       0        /* dead                 */
#define POS_MORTALLYW  1        /* mortally wounded     */
#define POS_STUNNED    2        /* stunned              */
#define POS_SLEEPING   3        /* sleeping             */
#define POS_LYING      4
#define POS_RESTING    5        /* resting              */
#define POS_SITTING    6        /* sitting              */
#define POS_FIGHTING   7        /* fighting             */
#define POS_STANDING   8        /* standing             */

/* attribute defines */

#define ATT_BOD         400
#define ATT_QUI         401
#define ATT_STR         402
#define ATT_CHA         403
#define ATT_INT         404
#define ATT_WIL         405
#define ATT_MAG         406
#define ATT_REA         407
#define ATT_ESS         408

/* attributes (mostly for trainers) */
#define BOD    0
#define QUI    1
#define STR    2
#define CHA    3
#define INT    4
#define WIL    5
#define REA    6
#define NUM_ATTRIBUTES 7

#define TBOD             (1 << 0)
#define TQUI             (1 << 1)
#define TSTR             (1 << 2)
#define TCHA             (1 << 3)
#define TINT             (1 << 4)
#define TWIL             (1 << 5)
#define TESS             (1 << 6)
#define TREA             (1 << 7)
#define TMAG             (1 << 8)

/* char and mob related defines */

/* magical traditions */

#define TRAD_HERMETIC   0
#define TRAD_SHAMANIC   1
#define TRAD_MUNDANE    2
#define TRAD_ADEPT      4

#define ASPECT_FULL    0
#define ASPECT_CONJURER    1
#define ASPECT_SHAMANIST  2
#define ASPECT_SORCERER    3
#define ASPECT_ELEMEARTH  4
#define ASPECT_ELEMAIR    5
#define ASPECT_ELEMFIRE    6
#define ASPECT_ELEMWATER  7
#define ASPECT_EARTHMAGE  8
#define ASPECT_AIRMAGE  9
#define ASPECT_FIREMAGE  10
#define ASPECT_WATERMAGE  11

#define METAMAGIC_STAGE_LOCKED   0
#define METAMAGIC_STAGE_UNLOCKED 1
#define METAMAGIC_STAGE_LEARNED  2


/* totems */

#define TOTEM_UNDEFINED    0
#define TOTEM_BEAR         1
#define TOTEM_BUFFALO      2
#define TOTEM_CAT          3
#define TOTEM_COYOTE       4
#define TOTEM_DOG          5
#define TOTEM_DOLPHIN      6
#define TOTEM_EAGLE        7
#define TOTEM_GATOR        8
#define TOTEM_LION         9
#define TOTEM_MOUSE        10
#define TOTEM_OWL          11
#define TOTEM_RACCOON      12
#define TOTEM_RAT          13
#define TOTEM_RAVEN        14
#define TOTEM_SHARK        15
#define TOTEM_SNAKE        16
#define TOTEM_WOLF         17
#define TOTEM_BADGER       18
#define TOTEM_BAT          19
#define TOTEM_BOAR         20
#define TOTEM_BULL         21
#define TOTEM_CHEETAH      22
#define TOTEM_COBRA        23
#define TOTEM_CRAB         24
#define TOTEM_CROCODILE    25
#define TOTEM_DOVE         26
#define TOTEM_ELK          27
#define TOTEM_FISH         28
#define TOTEM_FOX          29
#define TOTEM_GECKO        30
#define TOTEM_GOOSE        31
#define TOTEM_HORSE        32
#define TOTEM_HYENA        33
#define TOTEM_JACKAL       34
#define TOTEM_JAGUAR       35
#define TOTEM_LEOPARD      36
#define TOTEM_LIZARD       37
#define TOTEM_MONKEY       38
#define TOTEM_OTTER        39
#define TOTEM_PARROT       40
#define TOTEM_POLECAT      41
#define TOTEM_PRAIRIEDOG   42
#define TOTEM_PUMA         43
#define TOTEM_PYTHON       44
#define TOTEM_SCORPION     45
#define TOTEM_SPIDER       46
#define TOTEM_STAG         47
#define TOTEM_TURTLE       48
#define TOTEM_WHALE        49
#define TOTEM_SUN          50
#define TOTEM_LOVER        51
#define TOTEM_SEDUCTRESS   52
#define TOTEM_SIREN        53
#define TOTEM_OAK          54
#define TOTEM_MOON         55
#define TOTEM_MOUNTAIN     56
#define TOTEM_SEA          57
#define TOTEM_STREAM       58
#define TOTEM_WIND         59
#define TOTEM_ADVERSARY    60
#define TOTEM_BACCHUS      61
#define TOTEM_CREATOR      62
#define TOTEM_DARKKING     63
#define TOTEM_DRAGONSLAYER 64
#define TOTEM_FIREBRINGER  65
#define TOTEM_HORNEDMAN    66
#define TOTEM_SEAKING      67
#define TOTEM_SKYFATHER    68
#define TOTEM_WILDHUNTSMAN 69
#define TOTEM_WISEWARRIOR  70
#define TOTEM_GREATMOTHER  71
#define TOTEM_MOONMAIDEN   72
#define TOTEM_TRICKSTER    73
#define NUM_TOTEMS         74

/* PC races */

#define RACE_UNDEFINED             1
#define RACE_HUMAN                 2
#define RACE_DWARF                 3
#define RACE_ELF                   4
#define RACE_ORK                   5
#define RACE_TROLL                 6
#define RACE_CYCLOPS               7
#define RACE_KOBOROKURU            8
#define RACE_FOMORI                9
#define RACE_MENEHUNE              10
#define RACE_HOBGOBLIN             11
#define RACE_GIANT                 12
#define RACE_GNOME                 13
#define RACE_ONI                   14
#define RACE_WAKYAMBI              15
#define RACE_OGRE                  16
#define RACE_MINOTAUR              17
#define RACE_SATYR                 18
#define RACE_NIGHTONE              19
#define RACE_DRYAD                 20
#define RACE_DRAGON                21
#define RACE_ELEMENTAL             22
#define RACE_SPIRIT                23
#define RACE_PC_CONJURED_ELEMENTAL 24
#define NUM_RACES                  24  /* This must be the NUMBER of races */

/* level definitions */

enum {
  LVL_MORTAL = 1,
  LVL_BUILDER,     // 2
  LVL_ARCHITECT,   // 3
  LVL_FIXER,       // 4
  LVL_CONSPIRATOR, // 5
  LVL_EXECUTIVE,   // 6, minimum level for invis
  LVL_DEVELOPER,   // 7
  LVL_VICEPRES,    // 8
  LVL_ADMIN,       // 9
  LVL_PRESIDENT,   // 10

  LVL_MAX = LVL_PRESIDENT,
  LVL_FREEZE = LVL_CONSPIRATOR
};

/* character equipment positions: used as index for char_data.equipment[] */
/* note: don't confuse these constants with the ITEM_ bitvectors
   which control the valid places you can wear a piece of equipment */

#define WEAR_LIGHT      0
#define WEAR_HEAD       1
#define WEAR_EYES       2
#define WEAR_EAR        3
#define WEAR_EAR2       4
#define WEAR_FACE        5
#define WEAR_NECK_1     6
#define WEAR_NECK_2     7
#define WEAR_BACK       8
#define WEAR_ABOUT      9
#define WEAR_BODY      10
#define WEAR_UNDER     11
#define WEAR_ARMS      12
#define WEAR_LARM      13
#define WEAR_RARM      14
#define WEAR_WRIST_R   15
#define WEAR_WRIST_L   16
#define WEAR_HANDS     17
#define WEAR_WIELD     18
#define WEAR_HOLD      19
#define WEAR_SHIELD    20
#define WEAR_FINGER_R  21
#define WEAR_FINGER_L  22
#define WEAR_FINGER3   23
#define WEAR_FINGER4   24
#define WEAR_FINGER5   25
#define WEAR_FINGER6   26
#define WEAR_FINGER7   27
#define WEAR_FINGER8   28
#define WEAR_BELLY     29
#define WEAR_WAIST     30
#define WEAR_THIGH_R   31
#define WEAR_THIGH_L   32
#define WEAR_LEGS      33
#define WEAR_LANKLE    34
#define WEAR_RANKLE    35
#define WEAR_SOCK      36
#define WEAR_FEET      37
#define WEAR_PATCH     38
#define WEAR_UNDERWEAR 39
#define WEAR_CHEST     40
#define WEAR_LAPEL     41
#define NUM_WEARS      42

/* player flags: used by char_data.char_specials.act */
#define PLR_KILLER                          0  /* Player is a player-killer              */
#define PLR_BLACKLIST                       1  /* Player is banned from runs             */
#define PLR_FROZEN                          2  /* Player is frozen                       */
#define PLR_DONTSET                         3  /* Don't EVER set (ISNPC bit)             */
#define PLR_NEWBIE                          4  /* Player is a newbie still               */
#define PLR_JUST_DIED                       5  /* Player just died                       */
#define PLR_VISA                            6  /* Player needs to be crash-saved         */
#define PLR_RECEIVED_CYBEREYE_ESSENCE_DELTA 7  /* Player has had their cybereye essence cost reset */
#define PLR_SITEOK                          8  /* Player has been site-cleared           */
#define PLR_NOSHOUT                         9  /* Player not allowed to shout/goss       */
#define PLR_NOTITLE                         10 /* Player not allowed to set title        */
#define PLR_DELETED                         11 /* Player deleted - space reusable        */
#define PLR_NODELETE                        12 /* Player shouldn't be deleted            */
#define PLR_NOSTAT                          14 /* Player cannot be statted, etc          */
#define PLR_IN_CHARGEN                      15 /* Player is currently in chargen         */
#define PLR_INVSTART                        16 /* Player should enter game wizinvis      */
#define PLR_OLC                             19 /* Player has access to olc commands      */
#define PLR_MATRIX                          20 /* Player is in the Matrix                */
#define PLR_PERCEIVE                        21 /* player is astrally perceiving          */
#define PLR_PROJECT                         22 /* player is astrally projecting          */
#define PLR_SWITCHED                        23 /* player is switched to a mob            */
#define PLR_WRITING                         24 /* Player writing (board/mail/olc)        */
#define PLR_MAILING                         25 /* Player is writing mail                 */
#define PLR_EDITING                         26 /* Player is zone editing                 */
#define PLR_SPELL_CREATE                    27 /* Player is creating a spell             */
#define PLR_CUSTOMIZE                       28 /* Player is customizing persona          */
#define PLR_NOSNOOP                         29 /* Player is not snoopable                */
#define PLR_WANTED                          30 /* Player wanted by the law      */
#define PLR_NOOOC                           31 /* Player is muted from the OOC channel   */
#define PLR_NOT_YET_AUTHED                  32 /* Player needs Auth */
#define PLR_EDCON                           33
#define PLR_REMOTE                          34
#define PLR_INITIATE                        35
#define PLR_DRIVEBY                         36
#define PLR_RPE                             37
#define PLR_NO_IDLE_OUT                     38 /* Player will not idle out (morts- for test chars) */
#define PLR_TELLS_MUTED                     39 /* Remove their ability to send tells. */
#define PLR_NEWBIE_MUTED                    40 /* Remove their ability to talk on the newbie channel. */
#define PLR_CYBERDOC                        41 /* Player may act as a cyberdoc. */
#define PLR_PAID_FOR_CLOSECOMBAT            42
#define PLR_PAID_FOR_KIPUP                  43
#define PLR_PAID_FOR_ROLLS                  44
#define PLR_NO_AUTO_SYSP_AWARDS             45 /* Player abuses typo etc for automatic sysp awards. */
#define PLR_RADIO_MUTED                     46
#define PLR_SITE_HIDDEN                     47 /* For staff who really don't want people knowing who their alt is. */
#define PLR_ENABLED_DRUGS                   48
#define PLR_SENT_DOCWAGON_PLAYER_ALERT      49
#define PLR_PAID_FOR_VNUMS                  50
#define PLR_DOCWAGON_READY                  51 /* Flags if a player has rolled successfully for DocWagon. */
#define PLR_MAX                             52
// Adding something here? Add it to constants.cpp's player_bits too.



/* mobile flags: used by char_data.char_specials.act */

#define MOB_SPEC                 0  /* Mob has a callable spec-proc           */
#define MOB_SENTINEL             1  /* Mob should not move                    */
#define MOB_SCAVENGER            2  /* Mob picks up stuff on the ground       */
#define MOB_ISNPC                3  /* (R) Automatically set on all Mobs      */
#define MOB_AWARE                4  /* Mob can't be backstabbed               */
#define MOB_AGGRESSIVE           5  /* Mob hits players in the room           */
#define MOB_STAY_ZONE            6  /* Mob shouldn't wander out of zone       */
#define MOB_WIMPY                7  /* Mob flees if severely injured          */
#define MOB_AGGR_ORK             8  /* auto attack ork PC's                   */
#define MOB_AGGR_ELF             9  /* auto attack elf PC's                   */
#define MOB_AGGR_DWARF           10 /* auto attack dwarf PC's                 */
#define MOB_MEMORY               11 /* remember attackers if attacked         */
#define MOB_HELPER               12 /* attack PCs fighting other NPCs         */
#define MOB_NORAM                13 /* Mob can't be charmed                   */
#define MOB_DUAL_NATURE          14 /* mob is dual-natured                    */
#define MOB_IMMEXPLODE           15 /* mob is immune to explosions            */
#define MOB_AGGR_TROLL           16 /* auto attack troll PC's                 */
#define MOB_NOBLIND              17 /* Mob can't be blinded                   */
#define MOB_ASTRAL               18 /* Mob is solely in the astral plane      */
#define MOB_GUARD                19 /* mob carries out security               */
#define MOB_AGGR_HUMAN           20 /* auto attack human PC's                 */
#define MOB_SNIPER               21 /* mob searches area for PCs              */
#define MOB_PRIVATE              22 /* mob cannot be statted                  */
#define MOB_TRACK                23 /* (R) for security routines              */
#define MOB_FLAMEAURA            24
#define MOB_SPIRITGUARD          25
#define MOB_STUDY                26 /* Saeder Krupp*/
#define MOB_AIDSORCERY           27
#define MOB_AZTECHNOLOGY         28 /* Azzies                                 */
#define MOB_RENRAKU              29 /* Renraku   */
#define MOB_NOKILL               30 /* Unkillable mob */
#define MOB_TOTALINVIS           31 /* auto attack dragon PCs                 */
#define MOB_INANIMATE            32
#define MOB_EMPLACED             33 /* Won't close distance, can't be knocked down, has no recoil penalties, has unlimited ammo. Think emplaced turret. */
#define MOB_RACIAL_AGGR_VS_MOBS  34 /* Will trigger racial aggro vs other NPCs */
#define MOB_NO_NUYEN_LOOT_DROPS  35 /* Will never drop nuyen on death. */
#define MOB_PERCEIVING           36 /* Astrally perceiving */
#define MOB_MAX                  37

/* preference flags: used by char_data.player_specials.pref */

// If you add to this list, also add to constant.cpp's preference_bits[].
#define PRF_PACIFY                             0
#define PRF_COMPACT                            1
#define PRF_AUTOEXIT                           2  /* Display exits in a room       */
#define PRF_FIGHTGAG                           3  /* Gag extra fight messages  */
#define PRF_MOVEGAG                            4  /* Gag extra movement messages    */
#define PRF_DEAF                               5  /* Can't hear shouts     */
#define PRF_NOTELL                             6  /* Can't receive tells     */
#define PRF_NORADIO                            7  /* Can't hear radio frequencies     */
#define PRF_NONEWBIE                           8  /* Can't hear newbie channel    */
#define PRF_NOREPEAT                           9  /* No repetition of comm commands  */
#define PRF_HARDCORE                           10
#define PRF_PKER                               11 /* is able to pk/be pked        */
#define PRF_QUEST                              12 /* On quest        */
#define PRF_AFK                                13 /* Afk   */
#define PRF_SUPPRESS_STAFF_RADIO               14 /* EMPTY SPACE, FILL ME! */
#define PRF_UNUSED2_PLS_REPLACE                15 /* EMPTY SPACE, FILL ME!        15 */
#define PRF_NOHASSLE                           16 /* Aggr mobs won't attack  */
#define PRF_ROOMFLAGS                          17 /* Can see room flags (ROOM_x) */
#define PRF_HOLYLIGHT                          18 /* Can see in dark   */
#define PRF_CONNLOG                            19 /* Views ConnLog      */
#define PRF_DEATHLOG                           20 /* Views DeathLog        */
#define PRF_MISCLOG                            21 /* Views MiscLog          */
#define PRF_WIZLOG                             22 /* Views WizLog          */
#define PRF_SYSLOG                             23 /* Views SysLog         */
#define PRF_ZONELOG                            24 /* Views ZoneLog          */
#define PRF_LONGEXITS                          25
#define PRF_ROLLS                              26 /* sees details on rolls        */
#define PRF_NOOOC                              27 /* can't hear ooc channel      */
#define PRF_AUTOINVIS                          28 /* to toggle auto-invis for immortals    */
#define PRF_CHEATLOG                           29 /* Views CheatLog         */
#define PRF_ASSIST                             30 /* auto assist */
#define PRF_BANLOG                             31
#define PRF_NORPE                              32
#define PRF_NOHIRED                            33
#define PRF_GRIDLOG                            34
#define PRF_WRECKLOG                           35
#define PRF_QUESTOR                            36
#define PRF_NEWBIEHELPER                       37
#define PRF_MENUGAG                            38
#define PRF_LONGWEAPON                         39
#define PRF_PGROUPLOG                          40
#define PRF_SHOWGROUPTAG                       41
#define PRF_KEEPALIVE                          42
#define PRF_SCREENREADER                       43
#define PRF_NOCOLOR                            44
#define PRF_NOPROMPT                           45
#define PRF_HELPLOG                            46
#define PRF_PURGELOG                           47
#define PRF_NOAUTOKILL                         48
#define PRF_NO_RADIO_NAMES                     49
#define PRF_FUCKUPLOG                          50
#define PRF_ECONLOG                            51
#define PRF_BRIEF                              52
#define PRF_NOHIGHLIGHT                        53
#define PRF_NOPSEUDOLANGUAGE                   54
#define PRF_NO_IDLE_NUYEN_REWARD_MESSAGE       55
#define PRF_TOUCH_ME_DADDY                     56 /* Allows player cyberdocs to operate on you. Don't @ me. */
#define PRF_NO_VOID_ON_IDLE                    57
#define PRF_RADLOG                             58
#define PRF_ANONYMOUS_ON_WHERE                 59
#define PRF_IGNORELOG                          60
#define PRF_SEE_TIPS                           61
#define PRF_AUTOSTAND                          62
#define PRF_AUTOKIPUP                          63
#define PRF_NO_WEATHER                         64
#define PRF_DISABLE_XTERM                      65
#define PRF_COERCE_ANSI                        66
#define PRF_DONT_ALERT_PLAYER_DOCTORS_ON_MORT  67
#define PRF_MAILLOG                            68
#define PRF_MAX                                69

/* log watch */

#define LOG_CONNLOG        0
#define LOG_DEATHLOG       1
#define LOG_MISCLOG        2
#define LOG_WIZLOG         3
#define LOG_SYSLOG         4
#define LOG_ZONELOG        5
#define LOG_CHEATLOG       6
#define LOG_WIZITEMLOG     7
#define LOG_BANLOG         8
#define LOG_GRIDLOG         9
#define LOG_WRECKLOG       10
#define LOG_PGROUPLOG      11
#define LOG_HELPLOG        12
#define LOG_PURGELOG       13
#define LOG_FUCKUPLOG      14
#define LOG_ECONLOG        15
#define LOG_RADLOG         16
#define LOG_IGNORELOG      17
#define LOG_MAILLOG        18
#define NUM_LOGS           19
// If you add to this list:
//  - add your bit to act.other.cpp's 'skip log bits' section.
//  - add the name of your new log type to constants.cpp's log_types[].
//  - add it to utils.cpp's mudlog() switch.
//  - add it to act.wizard.cpp's do_logwatch().
//  - Make sure there's a corresponding PRF for it.

/* player conditions */

#define COND_DRUNK        0
#define COND_FULL         1
#define COND_THIRST       2


#define COND_IS_DISABLED  -1

#define MIN_FULLNESS      0
#define MIN_QUENCHED      0  // Note that 'quenched' is the inversion of thirst.
#define MIN_DRUNK         0

#define MAX_FULLNESS      20
#define MAX_QUENCHED      20
#define MAX_DRUNK         10

/* affect bits: used in char_data.char_specials.saved.affected_by */
/* WARNING: In the world files, NEVER set the bits marked "R" ("Reserved") */

#define AFF_INVISIBLE                           1  /* Char is invisible        */
#define AFF_BANISH                              2
#define AFF_ULTRASOUND                          3  /* Char can see invis chars    */
#define AFF_PRONE                               4
#define AFF_MANIFEST                            5
#define AFF_HEALED                              6
#define AFF_GROUP                               7  /* (R) Char is grouped       */
#define AFF_DAMAGED                             8
#define AFF_INFRAVISION                         9  /* Char can see in dark        */
#define AFF_FEAR                                10
#define AFF_RESISTPAIN                          11
#define AFF_DETOX                               12
#define AFF_LOW_LIGHT                           13 /* Char has low light eyes       */
#define AFF_LASER_SIGHT                         14 /* Char using laser sight       */
#define AFF_SNEAK                               15 /* Char can move quietly           */
#define AFF_HIDE                                16 /* Char is hidden                */
#define AFF_VISION_MAG_1                        17 /* Magnification level 1        */
#define AFF_CHARM                               18 /* Char is charmed              */
#define AFF_ACTION                              19 /* Player gets -10 on next init roll     */
#define AFF_VISION_MAG_2                        20 /* Magnification level 2         */
#define AFF_VISION_MAG_3                        21 /* Magnification level 3         */
#define AFF_COUNTER_ATT                         22 /* plr just attacked          */
#define AFF_STABILIZE                           23 /* player won't die due to phys loss */
#define AFF_RITUALCAST                          24
#define AFF_IMP_INVIS                           25 /* char is improved invis         */
#define AFF_ACID                                26
#define AFF_APPROACH                            27 /* Character is using melee */
#define AFF_PILOT                               28 /* Char is piloting a vehicle */
#define AFF_RIG                                 29
#define AFF_MANNING                             30 /* Char is in a (mini)turret */
#define AFF_DESIGN                              31
#define AFF_PROGRAM                             32
#define AFF_PART_DESIGN                         33
#define AFF_PART_BUILD                          34
#define AFF_WITHDRAWAL_FORCE                    35
#define AFF_WITHDRAWAL                          36
#define AFF_BONDING                             37
#define AFF_CONJURE                             38
#define AFF_LODGE                               39
#define AFF_CIRCLE                              40
#define AFF_PACKING                             41
#define AFF_SPELLINVIS                          42
#define AFF_SPELLIMPINVIS                       43
#define AFF_TRACKING                            44
#define AFF_TRACKED                             45
#define AFF_BINDING                             46
#define AFF_SPELLDESIGN                         47
#define AFF_SURPRISE                            48
#define AFF_AMMOBUILD                           49
#define AFF_CLOSECOMBAT                         50
#define AFF_SMART_ENOUGH_TO_TOGGLE_CLOSECOMBAT  51
#define AFF_LEVITATE                            52
#define AFF_FLAME_AURA                          53
#define AFF_VOICE_MODULATOR                     54
#define AFF_WEARING_ACTIVE_DOCWAGON_RECEIVER    55
#define AFF_MAX                                 56
// TODO: If you add another long-state action like building, designing, etc:
// - Add it to the BR_TASK_AFF_FLAGS section below, which affects bioware_check and the B/R flag in the wholist
// - Add it to the IS_WORKING and STOP_WORKING macros in utils.h
// - Check for anywhere I've missed in this comment
#define BR_TASK_AFF_FLAGS AFF_DESIGN, AFF_PROGRAM, AFF_PART_DESIGN, AFF_PART_BUILD, AFF_SPELLDESIGN, AFF_AMMOBUILD, AFF_RITUALCAST, AFF_CIRCLE, AFF_LODGE


/* room-related defines */

/* The cardinal directions: used as index to room_data.dir_option[] */

#define NORTH          0
#define NORTHEAST      1
#define EAST           2
#define SOUTHEAST      3
#define SOUTH          4
#define SOUTHWEST      5
#define WEST           6
#define NORTHWEST      7
#define UP             8
#define DOWN           9

#define NUM_OF_DIRS     10      /* number of directions in a room (nsewud) */
/* also, ne, se, sw, nw, and matrix        */

/* "extra" bitvector settings for movement */
#define CHECK_SPECIAL   1
#define LEADER          2

/* Room flags: used in room_data.room_flags */

/* WARNING: In the world files, NEVER set the bits marked "R" ("Reserved") */
#define ROOM_DARK                       0   /* Dark                      */
#define ROOM_DEATH                      1   /* Death trap                */
#define ROOM_NOMOB                      2   /* MOBs not allowed          */
#define ROOM_INDOORS                    3   /* Indoors                   */
#define ROOM_PEACEFUL                   4   /* Violence not allowed      */
#define ROOM_SOUNDPROOF                 5   /* Shouts, gossip blocked    */
#define ROOM_NOTRACK                    6   /* Track won't go through    */
#define ROOM_NOMAGIC                    7   /* Magic not allowed         */
#define ROOM_TUNNEL                     8   /* room for only 1 pers      */
#define ROOM_ARENA                      9   /* Can't teleport in         */
#define ROOM_STREETLIGHTS               10  /* Room has a streetlight    */
#define ROOM_HOUSE                      11  /* (R) Room is a house       */
#define ROOM_HOUSE_CRASH                12  /* (R) House needs saving    */
#define ROOM_ATRIUM                     13  /* (R) The door to a house   */
#define ROOM_OLC                        14  /* (R) Modifyable/!compress  */
#define ROOM_BFS_MARK                   15  /* (R) breath-first srch mrk */
#define ROOM_LOW_LIGHT                  16  /* Room viewable with ll-eyes */
#define ROOM_NO_RADIO                   18  /* Radio is sketchy and phones dont work */
#define ROOM_NOBIKE                     19  // Room blocks bikes from passing through it.
#define ROOM_FREEWAY                    20  /* Room cannot be walked across. */
#define ROOM_FALL                       21  // room is a 'fall' room
#define ROOM_ROAD                       22  // Room is compatible with cars.
#define ROOM_GARAGE                     23  // Room stores cars.
#define ROOM_STAFF_ONLY                 24  // Room does not allow mortals to walk into it.
#define ROOM_NOQUIT                     25  // Room does not allow quitting in it.
#define ROOM_SENT                       26
#define ROOM_ASTRAL                     27 // Astral room
#define ROOM_NOGRID                     28 // Room blocks gridguide.
#define ROOM_STORAGE                    29 // Room stores items dropped in it.
#define ROOM_NO_TRAFFIC                 30 // Prevents display of traffic atmospheric messages.
#define ROOM_ELEVATOR_SHAFT             31 // Don't set this manually
#define ROOM_ENCOURAGE_CONGREGATION     32
#define ROOM_CORPSE_SAVE_HACK           33
#define ROOM_STERILE                    34 // Gives a bonus to medical actions.
#define ROOM_TOO_CRAMPED_FOR_CHARACTERS 35 // Prevents you from entering if you're not a projection or tiny drone.
#define ROOM_RADIATION                  36 // You take radiation damage while you're here, scaled by Rating.
#define ROOM_ALL_VEHICLE_ACCESS         37 // Bypasses land/air/water constraints.
#define ROOM_MAX                        38

#define VISION_NORMAL         0
#define VISION_LOWLIGHT       1
#define VISION_THERMOGRAPHIC  2
#define VISION_ULTRASONIC     3
#define NUM_VISION_TYPES      4

#define LIGHT_NORMAL    0
#define LIGHT_NORMALNOLIT  1
#define LIGHT_FULLDARK    2
#define LIGHT_MINLIGHT    3
#define LIGHT_PARTLIGHT    4
#define LIGHT_GLARE    5
#define LIGHT_MIST    6
#define LIGHT_LIGHTSMOKE  7
#define LIGHT_HEAVYSMOKE  8
#define LIGHT_THERMALSMOKE  9

/* Exit info: used in room_data.dir_option.exit_info */
#define EX_ISDOOR               (1 << 0)   /* Exit is a door            */
#define EX_CLOSED               (1 << 1)   /* The door is closed        */
#define EX_LOCKED               (1 << 2)   /* The door is locked        */
#define EX_PICKPROOF            (1 << 3)   /* Lock can't be picked      */
#define EX_DESTROYED            (1 << 4)   /* door has been destroyed   */
#define EX_HIDDEN               (1 << 5)   /* exit is hidden            */
#define EX_ASTRALLY_WARDED      (1 << 6)   /* Exit blocks passage of astral beings */
#define EX_IS_TEMPORARY         (1 << 7)   /* Created temporarily by code */
#define EX_WINDOWED             (1 << 8)   /* Exit has a window.        */
#define EX_BARRED_WINDOW        (1 << 9)   /* Exit has a barred window. */
#define NUM_EXFLAGS             11

/* spirit powers */

#define SERV_APPEAR    0
#define SERV_SORCERY    1
#define SERV_STUDY    2
#define SERV_SUSTAIN    3
#define SERV_ACCIDENT    4
#define SERV_BINDING    5
#define SERV_CONCEAL    6
#define SERV_CONFUSION    7
#define SERV_DEMATERIAL    8
#define SERV_ENGULF    9
#define SERV_FEAR    10
#define SERV_FLAMEAURA    11
#define SERV_FLAMETHROWER  12
#define SERV_GUARD    13
#define SERV_LEAVE    14
#define SERV_MATERIALIZE  15
#define SERV_MOVEMENT    16
#define SERV_BREATH    17
#define SERV_PSYCHOKINESIS  18
#define SERV_SEARCH    19
#define SERV_ATTACK    20
#define NUM_SERVICES    21

#define SPELL_DEATHTOUCH  1
#define SPELL_MANABOLT    2
#define SPELL_MANABALL    3
#define SPELL_POWERBOLT    4
#define SPELL_POWERBALL    5
#define SPELL_STUNBOLT    6
#define SPELL_STUNBALL    7
#define SPELL_ANALYZEDEVICE  8
#define SPELL_CLAIRAUDIENCE  9
#define SPELL_CLAIRVOYANCE  10
#define SPELL_COMBATSENSE  11
#define SPELL_DETECTENEMIES  12
#define SPELL_DETECTINDIV  13
#define SPELL_DETECTLIFE  14
#define SPELL_DETECTMAGIC  15
#define SPELL_DETECTOBJECT  16
#define SPELL_MINDLINK    17
#define SPELL_DECATTR    18
#define SPELL_DECCYATTR    19
#define SPELL_DETOX    20
#define SPELL_HEAL    21
#define SPELL_TREAT    22
#define SPELL_HEALTHYGLOW  23
#define SPELL_INCATTR    24
#define SPELL_INCCYATTR    25
#define SPELL_INCREA    26
#define SPELL_INCREF1    27
#define SPELL_INCREF2    28
#define SPELL_INCREF3    29
#define SPELL_PROPHYLAXIS  30
#define SPELL_RESISTPAIN  31
#define SPELL_STABILIZE    32
#define SPELL_CONFUSION    33
#define SPELL_MASSCONFUSION  34
#define SPELL_CHAOS    35
#define SPELL_CHAOTICWORLD  36
#define SPELL_INVIS    37
#define SPELL_IMP_INVIS    38
#define SPELL_MASK    39
#define SPELL_PHYSMASK    40
#define SPELL_SILENCE    41
#define SPELL_STEALTH    42
#define SPELL_ACIDSTREAM  43
#define SPELL_TOXICWAVE    44
#define SPELL_FLAMETHROWER  45
#define SPELL_FIREBALL    46
#define SPELL_LIGHTNINGBOLT  47
#define SPELL_BALLLIGHTNING  48
#define SPELL_CLOUT    49
#define SPELL_POLTERGEIST  50
#define SPELL_ARMOR    51
#define SPELL_PHYSICALBARRIER  52
#define SPELL_ASTRALBARRIER  53
#define SPELL_ICESHEET    54
#define SPELL_IGNITE    55
#define SPELL_LIGHT    56
#define SPELL_SHADOW    57
#define SPELL_LASER    58
#define SPELL_NOVA    59
#define SPELL_STEAM    60
#define SPELL_SMOKECLOUD    61
#define SPELL_THUNDERBOLT    62
#define SPELL_THUNDERCLAP    63
#define SPELL_WATERBOLT    64
#define SPELL_SPLASH    65
#define SPELL_NIGHTVISION    66
#define SPELL_INFRAVISION    67
#define SPELL_LEVITATE    68
#define SPELL_FLAME_AURA    69
#define MAX_SPELLS    70

#define SPELL_DESIGN_FAILED_CODE -3

#define META_UNDEFINED    0
#define META_CENTERING    1
#define META_CLEANSING    2
#define META_INVOKING    3
#define META_MASKING    4
#define META_POSSESSING    5
#define META_QUICKENING    6
#define META_REFLECTING    7
#define META_SHIELDING    8
#define META_ANCHORING    9
#define META_MAX    10

#define MASK_INIT    (1 << 1)
#define MASK_DUAL    (1 << 2)
#define MASK_COMPLETE    (1 << 3)

#define AURA_VIOLENCE      0
#define AURA_TORTURE       1
#define AURA_HATRED        2
#define AURA_GENOCIDE      3
#define AURA_RITUAL_MAGIC  4
#define AURA_SACRIFICE     5
#define AURA_WORSHIP       6
#define AURA_DEATH         7
#define AURA_POLLUTION     8
#define AURA_CONCENTRATED  9
#define AURA_LOST_HUMANITY 10
#define AURA_STERILITY     11
#define AURA_CONFUSION     12
#define AURA_POWERSITE     13
#define AURA_BLOOD_MAGIC   14
#define AURA_PLAYERCOMBAT   15
#define AURA_PLAYERDEATH   16

#define COMBAT      1
#define DETECTION    2
#define HEALTH      3
#define ILLUSION    4
#define MANIPULATION    5

#define INSTANT      0
#define SUSTAINED    1
#define PERMANENT    2

#define SINGLE      0
#define AREA      1
#define TOUCH      2
#define CASTER      3

#define TYPE_LIBRARY_CONJURE    0
#define TYPE_LIBRARY_SPELL      1
#define TYPE_CIRCLE             2
#define TYPE_LODGE              3
#define TYPE_SUMMONING          4

#define SPIRIT_HEARTH           0
#define SPIRIT_CITY             1
#define SPIRIT_FIELD            2
#define SPIRIT_FOREST           3
#define SPIRIT_DESERT           4
#define SPIRIT_MOUNTAIN         5
#define SPIRIT_RIVER            6
#define SPIRIT_SEA              7
#define SPIRIT_PRAIRIE          8
#define SPIRIT_SKY    9
#define SPIRIT_MIST             9
#define SPIRIT_STORM            10
#define SPIRIT_WIND             11
#define SPIRIT_LAKE             12
#define SPIRIT_SWAMP            13
#define NUM_SPIRITS    14

#define ELEM_EARTH    0
#define ELEM_FIRE    1
#define ELEM_AIR    2
#define ELEM_WATER    3
#define NUM_ELEMENTS    4

#define ELEMAF_NONE    0
#define ELEMAF_ACID    1
#define ELEMAF_BLAST    2
#define ELEMAF_FIRE    3
#define ELEMAF_ICE    4
#define ELEMAF_LIGHT    5
#define ELEMAF_LIGHTNING  6
#define ELEMAF_METAL    7
#define ELEMAF_SAND    8
#define ELEMAF_SMOKE    9
#define ELEMAF_WATER    10
#define NUM_ELEMS    11

#define MENTAL       0
#define PHYSICAL    1
/* reserved skill defines */
#define TYPE_UNDEFINED               -1


#define MIN_SKILLS                   1
#define SKILL_ATHLETICS              1
#define SKILL_ARMED_COMBAT           2
#define SKILL_EDGED_WEAPONS          3
#define SKILL_POLE_ARMS              4
#define SKILL_WHIPS_FLAILS           5
#define SKILL_CLUBS                  6
#define SKILL_UNARMED_COMBAT         7
#define SKILL_CENTERING              8
#define SKILL_CYBER_IMPLANTS         9
#define SKILL_FIREARMS               10
#define SKILL_PISTOLS                11
#define SKILL_RIFLES                 12
#define SKILL_SHOTGUNS               13
#define SKILL_ASSAULT_RIFLES         14
#define SKILL_SMG                    15
#define SKILL_GRENADE_LAUNCHERS      16
#define SKILL_TASERS                 17
#define SKILL_GUNNERY                18
#define SKILL_MACHINE_GUNS           19
#define SKILL_MISSILE_LAUNCHERS      20
#define SKILL_ASSAULT_CANNON         21
#define SKILL_ARTILLERY              22
#define SKILL_PROJECTILES            23
#define SKILL_ORALSTRIKE             24
#define SKILL_SPELLDESIGN            25
#define SKILL_THROWING_WEAPONS       26
#define SKILL_ENCHANTING             27
#define SKILL_CYBERTERM_DESIGN       28
#define SKILL_DEMOLITIONS            29
#define SKILL_COMPUTER               30
#define SKILL_ELECTRONICS            31
#define SKILL_BR_COMPUTER            32
#define SKILL_BIOTECH                33
#define SKILL_BR_ELECTRONICS         34
#define SKILL_TALISMONGERING         35
#define SKILL_POLICE_PROCEDURES      36
#define SKILL_LEADERSHIP             37
#define SKILL_INTERROGATION          38
#define SKILL_NEGOTIATION            39
#define SKILL_CHANTING               40
#define SKILL_CONJURING              41
#define SKILL_SORCERY                42
#define SKILL_LEGERDEMAIN            43
#define SKILL_CORPORATE_ETIQUETTE    44
#define SKILL_MEDIA_ETIQUETTE        45
#define SKILL_STREET_ETIQUETTE       46
#define SKILL_TRIBAL_ETIQUETTE       47
#define SKILL_ELF_ETIQUETTE          48
#define SKILL_PROGRAM_COMBAT         49
#define SKILL_PROGRAM_DEFENSIVE      50
#define SKILL_PROGRAM_OPERATIONAL    51
#define SKILL_PROGRAM_SPECIAL        52
#define SKILL_PROGRAM_CYBERTERM      53
#define SKILL_DATA_BROKERAGE         54
#define SKILL_AURA_READING           55
#define SKILL_STEALTH                56
#define SKILL_STEAL                  57
#define SKILL_TRACK                  58
#define SKILL_UNUSED_WAS_CLIMBING    59
#define SKILL_PILOT_BIKE             60
#define SKILL_UNUSED_WAS_PILOT_FIXED_WING   61
#define SKILL_PILOT_CAR              62
#define SKILL_PILOT_TRUCK            63
#define SKILL_BR_BIKE                64
#define SKILL_BR_CAR                 65
#define SKILL_BR_DRONE               66
#define SKILL_BR_TRUCK               67
#define SKILL_DANCING                68
#define SKILL_SINGING                69
#define SKILL_INSTRUMENT             70
#define SKILL_ARCANELANGUAGE         71
#define SKILL_MEDITATION             72

#define SKILL_ENGLISH                73
#define SKILL_SPERETHIEL             74
#define SKILL_SPANISH                75
#define SKILL_JAPANESE               76
#define SKILL_CHINESE                77
#define SKILL_KOREAN                 78
#define SKILL_ITALIAN                79
#define SKILL_RUSSIAN                80
#define SKILL_SIOUX                  81
#define SKILL_MAKAW                  82
#define SKILL_CROW                   83
#define SKILL_SALISH                 84
#define SKILL_UTE                    85
#define SKILL_NAVAJO                 86
#define SKILL_GERMAN                 87
#define SKILL_ORZET                  88
#define SKILL_ARABIC                 89
#define SKILL_LATIN                  90
#define SKILL_GAELIC                 91
#define SKILL_FRENCH                 92
// Also see SKILL_SIGN_LANGUAGE and the other language additions below.

#define SKILL_ANIMAL_HANDLING           93
#define SKILL_ANIMAL_TAMING             94
#define SKILL_BR_EDGED                  95
#define SKILL_BR_POLEARM                96
#define SKILL_BR_CLUB                   97
#define SKILL_BR_THROWINGWEAPONS        98
#define SKILL_BR_WHIPS                  99
#define SKILL_BR_PROJECTILES            100
#define SKILL_BR_PISTOL                 101
#define SKILL_BR_SHOTGUN                102
#define SKILL_BR_RIFLE                  103
#define SKILL_BR_HEAVYWEAPON            104
#define SKILL_BR_SMG                    105
#define SKILL_BR_ARMOR                  106
#define SKILL_OFFHAND_EDGED             107
#define SKILL_OFFHAND_CLUB              108
#define SKILL_OFFHAND_CYBERIMPLANTS     109
#define SKILL_OFFHAND_WHIP              110
#define SKILL_SURVIVAL                  111
#define SKILL_NAVI_LAND                 112
#define SKILL_NAVI_WATER                113
#define SKILL_NAVI_AIR                  114
#define SKILL_SMALL_UNIT_TACTICS        115
#define SKILL_CHEMISTRY                 116
#define SKILL_DIVING                    117
#define SKILL_PARACHUTING               118
#define SKILL_UNDERWATER_COMBAT         119
#define SKILL_PILOT_ROTORCRAFT          120
#define SKILL_PILOT_FIXEDWING           121
#define SKILL_PILOT_VECTORTHRUST        122
#define SKILL_ACTING                    123
#define SKILL_DISGUISE                  124
#define SKILL_LOCK_PICKING              125
#define SKILL_RIDING                    126
#define SKILL_SPRAY_WEAPONS             127
#define SKILL_INTIMIDATION              128
#define SKILL_GUNCANE                   129
#define SKILL_BRACERGUN                 130
#define SKILL_BLOWGUN                   131
#define SKILL_PHARMA                    132
#define SKILL_HEBREW                    133
#define SKILL_IROQUOIS                  134
#define SKILL_MEDICINE                  135
#define SKILL_BR_FIXEDWING              136
#define SKILL_BR_ROTORCRAFT             137
#define SKILL_BR_VECTORTHRUST           138
#define SKILL_BR_HOVERCRAFT             139
#define SKILL_BR_MOTORBOAT              140
#define SKILL_BR_SHIP                   141
#define SKILL_BR_LTA                    142
#define SKILL_PILOT_HOVERCRAFT          143
#define SKILL_PILOT_MOTORBOAT           144
#define SKILL_PILOT_SHIP                145
#define SKILL_PILOT_LTA                 146
#define SKILL_MECHANICAL_ARM_OPERATION  147
#define SKILL_PILOT_SEMIBALLISTIC       148
#define SKILL_PILOT_SUBORBITAL          149
#define SKILL_PILOT_TRACKED             150
#define SKILL_PILOT_WALKER              151

#define MAX_SKILLS                152
// Adding a pilot skill? Update utils.cpp's pilot_skills[].

// Skill type definitions.
#define SKILL_TYPE_ACTIVE         0
#define SKILL_TYPE_KNOWLEDGE      1

/* TODO: Not yet implemented.
#define SKILL_SIGN_LANGUAGE       133
#define SKILL_IMMORTAL_LANGUAGE   134
#define MAX_SKILLS      135
*/

// Defines the size of the teach_t array. Changing this means you have to change every teacher too.
#define NUM_TEACHER_SKILLS 25

#define ADEPT_MIN_POWER   1
#define ADEPT_PERCEPTION  1
#define ADEPT_COMBAT_SENSE  2
#define ADEPT_BLIND_FIGHTING  3
#define ADEPT_QUICK_STRIKE  4
#define ADEPT_KILLING_HANDS  5
#define ADEPT_NERVE_STRIKE  6
#define ADEPT_SMASHING_BLOW  7
#define ADEPT_DISTANCE_STRIKE  8
#define ADEPT_REFLEXES    9
#define ADEPT_BOOST_STR    10
#define ADEPT_BOOST_QUI    11
#define ADEPT_BOOST_BOD    12
#define ADEPT_IMPROVED_STR  13
#define ADEPT_IMPROVED_QUI  14
#define ADEPT_IMPROVED_BOD  15
#define ADEPT_IMPROVED_PERCEPT  16
#define ADEPT_LOW_LIGHT    17
#define ADEPT_THERMO    18
#define ADEPT_IMAGE_MAG    19
#define ADEPT_MAGIC_RESISTANCE  20
#define ADEPT_PAIN_RESISTANCE  21
#define ADEPT_TEMPERATURE_TOLERANCE  22
#define ADEPT_SPELL_SHROUD  23
#define ADEPT_TRUE_SIGHT  24
#define ADEPT_MISSILE_PARRY  25
#define ADEPT_MISSILE_MASTERY  26
#define ADEPT_MYSTIC_ARMOR  27
#define ADEPT_HEALING    28
#define ADEPT_FREEFALL    29
#define ADEPT_IRONWILL    30
#define ADEPT_AIDSPELL    31
#define ADEPT_EMPATHICHEAL  32
#define ADEPT_KINESICS    33
#define ADEPT_LINGUISTICS  34
#define ADEPT_LIVINGFOCUS  35
#define ADEPT_MOTIONSENSE  36
#define ADEPT_PAINRELIEF  37
#define ADEPT_PENETRATINGSTRIKE  38
#define ADEPT_SIDESTEP    39
#define ADEPT_SUSTENANCE  40
#define ADEPT_COUNTERSTRIKE  41
#define ADEPT_ANIMAL_EMPATHY    42 // NERP Adept Powers from here on - Vile
#define ADEPT_BODY_CONTROL    43
#define ADEPT_COMMANDING_VOICE    44
#define ADEPT_COOL_RESOLVE    45
#define ADEPT_DEEP_ROOTING    46
#define ADEPT_DELAY_DAMAGE_OBVIOUS    47
#define ADEPT_DELAY_DAMAGE_SILENT    48
#define ADEPT_EIDETIC_MEMORY    49
#define ADEPT_ELEMENTAL_STRIKE    50
#define ADEPT_EMPATHIC_READING    51
#define ADEPT_EMPATHIC_SENSE    52
#define ADEPT_ENHANCED_BALANCE    53
#define ADEPT_ENTHRALLING_PERFORMANCE    54
#define ADEPT_FACIAL_SCULPT    55
#define ADEPT_FLEXIBILITY    56
#define ADEPT_GLIDING    57
#define ADEPT_GREAT_LEAP    58
#define ADEPT_FLARE    59
#define ADEPT_HIGH_FREQ    60
#define ADEPT_LOW_FREQ    61
#define ADEPT_DAMPENING    62
#define ADEPT_FILTERING    63
#define ADEPT_SPATIAL    64
#define ADEPT_HEARING    65
#define ADEPT_DIRECTIONAL    66
#define ADEPT_SCENT    67
#define ADEPT_TASTE    68
#define ADEPT_INERTIA_STRIKE    69
#define ADEPT_IRON_GUT    70
#define ADEPT_IRON_LUNGS    71
#define ADEPT_MAGIC_SENSE    72
#define ADEPT_MELANIN_CONTROL    73
#define ADEPT_MULTI_TASKING    74
#define ADEPT_NIMBLE_FINGERS    75
#define ADEPT_QUICK_DRAW    76
#define ADEPT_RESILIENCE    77
#define ADEPT_ROOTING    78
#define ADEPT_SIXTH_SENSE    79
#define ADEPT_SPRINT    80
#define ADEPT_SUSPENDED_STATE    81
#define ADEPT_THREE_D_MEM    82
#define ADEPT_TRACELESS_WALK    83
#define ADEPT_VOICE_CONTROL    84
#define ADEPT_WALL_RUNNING    85
#define ADEPT_NUMPOWER    86

/* ban defines -- do not change */

#define BAN_NOT         0
#define BAN_NEW         1
#define BAN_SELECT      2
#define BAN_ALL         3

#define BANNED_SITE_LENGTH    50 // if you change this, update fscanf in ban.cpp

/* weapon attack types */

#define TYPE_HIT              300     // melee
#define TYPE_STING            301
#define TYPE_WHIP             302
#define TYPE_SLASH            303
#define TYPE_BITE             304
#define TYPE_BLUDGEON         305
#define TYPE_CRUSH            306
#define TYPE_POUND            307
#define TYPE_CLAW             308
#define TYPE_MAUL             309
#define TYPE_GORE             310    // cyberhorns - Vile
#define TYPE_PIERCE           311
#define TYPE_PUNCH            312
#define TYPE_STAB             313
#define TYPE_TASER            314
#define TYPE_SHURIKEN         315    // throwing weapons - 1 room
#define TYPE_THROWING_KNIFE   316
#define TYPE_ARROW            317
#define TYPE_HAND_GRENADE     318    // explosive weapons
#define TYPE_GRENADE_LAUNCHER 319
#define TYPE_ROCKET           320
#define TYPE_PISTOL           321    // gun
#define TYPE_BLAST            322
#define TYPE_RIFLE            323
#define TYPE_SHOTGUN          324
#define TYPE_MACHINE_GUN      325
#define TYPE_CANNON           326
#define TYPE_BIFURCATE        327
#define TYPE_CRASH            328
#define TYPE_DUMPSHOCK        329
#define TYPE_BLACKIC          330
#define TYPE_SUFFERING        399
#define TYPE_EXPLOSION        400
#define TYPE_SCATTERING       401
#define TYPE_FALL             402
#define TYPE_DROWN            403
#define TYPE_ALLERGY          404
#define TYPE_BIOWARE          405
#define TYPE_RECOIL           406
#define TYPE_RAM              407
#define TYPE_DRAIN            408
#define TYPE_FUMES            409
#define TYPE_FIRE             410
#define TYPE_ACID             411
#define TYPE_POLTERGEIST      412
#define TYPE_ELEVATOR         413
#define TYPE_MEDICAL_MISHAP   414
#define TYPE_SPELL_DRAIN      415
#define TYPE_DRUGS            416
#define TYPE_POISON           417

/* magic attack types */
#define TYPE_MANABOLT_OR_STUNBOLT       500
#define TYPE_POWERBOLT                  501
#define TYPE_MANIPULATION_SPELL         502

/* used in void hit, for counter attacks */
#define TYPE_MELEE 1234

#define WEAP_EDGED          0
#define WEAP_CLUB           1
#define WEAP_POLEARM        2
#define WEAP_WHIP           3
#define WEAP_GLOVE          4
#define WEAP_HOLDOUT        5
#define WEAP_LIGHT_PISTOL   6
#define WEAP_MACHINE_PISTOL 7
#define WEAP_HEAVY_PISTOL   8
#define WEAP_TASER          9
#define WEAP_SMG            10
#define WEAP_SPORT_RIFLE    11
#define WEAP_SNIPER_RIFLE   12
#define WEAP_ASSAULT_RIFLE  13
#define WEAP_SHOTGUN        14
#define WEAP_LMG            15
#define WEAP_MMG            16
#define WEAP_HMG            17
#define WEAP_CANNON         18
#define WEAP_MINIGUN        19
#define WEAP_GREN_LAUNCHER  20
#define WEAP_MISS_LAUNCHER  21
#define WEAP_REVOLVER       22
#define WEAP_GRENADE        23
#define MAX_WEAP            24

// Defines our weapon ranges for bullet pants. Holdout -> revolver.
#define START_OF_AMMO_USING_WEAPONS 5
#define END_OF_AMMO_USING_WEAPONS   22

/* all those attack types can be used plus these for damage types to
* objects */

// Grenade types. MUST start from -3 and go DOWN from there for attachment compat reasons.
#define GRENADE_TYPE_EXPLOSIVE  -3
#define GRENADE_TYPE_FLASHBANG -4

/* item types: used by obj_data.obj_flags.type_flag */

#define MIN_ITEM                1        /* Must always be the minimum valid item-type number. */
#define ITEM_LIGHT              1        /* Item is a light source            */
#define ITEM_WORKSHOP           2        /* Item is a workshop (veh, etc)     */
#define ITEM_CAMERA             3        /* Item is a camera                  */
#define ITEM_PART               4        /* Item is a component of something  */
#define ITEM_WEAPON             5        /* Item is a weapon                  */
#define ITEM_FIREWEAPON         6        /* Item is bow/xbow                  */
#define ITEM_MISSILE            7        /* Item is arrow/bolt                */
#define ITEM_CUSTOM_DECK        8        /* Item is a custom cyberdeck        */
#define ITEM_GYRO               9        /* Item is Gyroscopic Harness        */
#define ITEM_DRUG               10       /* Item is a drug                    */
#define ITEM_WORN               11       /* Item is worn, includes armor      */
#define ITEM_OTHER              12       /* Misc object                       */
#define ITEM_MAGIC_TOOL         13       /* Item is a library, etc            */
#define ITEM_DOCWAGON           14       /* Item is a docwagon contract       */
#define ITEM_CONTAINER          15       /* Item is a container               */
#define ITEM_RADIO              16       /* Item is radio                     */
#define ITEM_DRINKCON           17       /* Item is a drink container         */
#define ITEM_KEY                18       /* Item is a key                     */
#define ITEM_FOOD               19       /* Item is food                      */
#define ITEM_MONEY              20       /* Item is money (nuyen/credstick)   */
#define ITEM_PHONE              21       /* Item is a phone                   */
#define ITEM_BIOWARE            22       /* Item is bioware                   */
#define ITEM_FOUNTAIN           23       /* Item is a fountain                */
#define ITEM_CYBERWARE          24       /* Item is cyberware                 */
#define ITEM_CYBERDECK          25       /* Item is a cyberdeck               */
#define ITEM_PROGRAM            26       /* Item is a program                 */
#define ITEM_GUN_MAGAZINE       27       /* Item is a gun magazine            */
#define ITEM_GUN_ACCESSORY      28       /* Item is a gun accessory           */
#define ITEM_SPELL_FORMULA      29       /* Item is a spell formula           */
#define ITEM_FOCUS              31       /* magical foci of various types     */
#define ITEM_PATCH              32       /* type of slap patch                */
#define ITEM_CLIMBING           33       /* climbing gear                     */
#define ITEM_QUIVER             34       /* holds projectiles                 */
#define ITEM_DECK_ACCESSORY     35       /* decking accessory                 */
#define ITEM_RCDECK             36       /* remote control deck               */
#define ITEM_CHIP               37
#define ITEM_MOD                38
#define ITEM_HOLSTER            39
#define ITEM_DESIGN             40
#define ITEM_QUEST              41
#define ITEM_GUN_AMMO           42
#define ITEM_KEYRING            43
#define ITEM_SHOPCONTAINER      44
#define ITEM_VEHCONTAINER       45
#define ITEM_GRAFFITI           46
#define ITEM_DESTROYABLE        47
#define ITEM_LOADED_DECORATION  48
#define NUM_ITEMS               49
// Adding something? Add convenience definees to utils.hpp and put the type's name in constants.cpp.

#define PATCH_ANTIDOTE          0
#define PATCH_STIM              1
#define PATCH_TRANQ             2
#define PATCH_TRAUMA            3
#define NUM_PATCHES             4

#define DOCWAGON_GRADE_NOT_SET  0
#define DOCWAGON_GRADE_BASIC    1
#define DOCWAGON_GRADE_GOLD     2
#define DOCWAGON_GRADE_PLATINUM 3

/* take/wear flags: used by obj_data.obj_flags.wear_flags */

#define ITEM_WEAR_TAKE          0  /* Item can be taken          */
#define ITEM_WEAR_FINGER        1  /* Can be worn on finger      */
#define ITEM_WEAR_NECK          2  /* Can be worn around neck    */
#define ITEM_WEAR_BODY          3  /* Can be worn on body        */
#define ITEM_WEAR_HEAD          4  /* Can be worn on head        */
#define ITEM_WEAR_LEGS          5  /* Can be worn on legs        */
#define ITEM_WEAR_FEET          6  /* Can be worn on feet        */
#define ITEM_WEAR_HANDS         7  /* Can be worn on hands       */
#define ITEM_WEAR_ARMS          8  /* Can be worn on arms        */
#define ITEM_WEAR_SHIELD        9  /* Can be used as a shield    */
#define ITEM_WEAR_ABOUT         10 /* Can be worn about body     */
#define ITEM_WEAR_WAIST         11 /* Can be worn around waist   */
#define ITEM_WEAR_WRIST         12 /* Can be worn on wrist       */
#define ITEM_WEAR_WIELD         13 /* Can be wielded             */
#define ITEM_WEAR_HOLD          14 /* Can be held                */
#define ITEM_WEAR_EYES          15 /* worn on eyes          */
#define ITEM_WEAR_EAR           16 /* can be worn on/in ear  */
#define ITEM_WEAR_UNDER  17
#define ITEM_WEAR_BACK  18
#define ITEM_WEAR_ANKLE  19
#define ITEM_WEAR_SOCK  20
#define ITEM_WEAR_BELLY  21
#define ITEM_WEAR_ARM    22
#define ITEM_WEAR_FACE   23
#define ITEM_WEAR_THIGH  24
#define ITEM_WEAR_MAX    25

/* extra object flags: used by obj_data.obj_flags.extra_flags */
/* see: https://github.com/luciensadi/AwakeMUD/wiki/Item-Extra-Flags */

#define ITEM_EXTRA_GLOW               0     /* Item is glowing              */
#define ITEM_EXTRA_HUM                1     /* Item is humming              */
#define ITEM_EXTRA_NORENT             2     /* Item cannot be rented        */
#define ITEM_EXTRA_NODONATE           3     /* Item cannot be donated       */
#define ITEM_EXTRA_NOINVIS            4     /* Item cannot be made invis    */
#define ITEM_EXTRA_INVISIBLE          5     /* Item is invisible            */
#define ITEM_EXTRA_MAGIC              6     /* Item is magical              */
#define ITEM_EXTRA_NODROP             7     /* Item is cursed: can't drop   */
#define ITEM_EXTRA_FORMFIT            8     /* Item is blessed              */
#define ITEM_EXTRA_NOSELL             9     /* Shopkeepers won't touch it   */
#define ITEM_EXTRA_CORPSE             10    /* Item is a corpse             */
#define ITEM_EXTRA_STAFF_ONLY         11    /* Only a god may use this item */
#define ITEM_EXTRA_TWOHANDS           12    /* weapon takes 2 hands to use */
#define ITEM_EXTRA_COMPBURST          13    /* Weapon requires complex action to use burst fire */
#define ITEM_EXTRA_VOLATILE           14    /* connected item loaded in ip zone */
#define ITEM_EXTRA_WIZLOAD            15    /* item was loaded by an immortal */
#define ITEM_EXTRA_NOTROLL            16
#define ITEM_EXTRA_NOELF              17
#define ITEM_EXTRA_NODWARF            18
#define ITEM_EXTRA_NOORK              19
#define ITEM_EXTRA_NOHUMAN            20
#define ITEM_EXTRA_SNIPER             21
// #define ITEM_EXTRA_IMMLOAD            22  not currently used, BUT reserved since many items have this set.
#define ITEM_EXTRA_NERPS              23    /* Item does not actually have any coded effect. */
#define ITEM_EXTRA_BLOCKS_ARMOR       24    // Can't wear other armors with this.
#define ITEM_EXTRA_HARDENED_ARMOR     25    // Applies hardened armor rules (deflect attacks with power <= armor rating) CC p51
#define ITEM_EXTRA_DONT_TOUCH         26    // Warns strenuously on editing.
#define ITEM_EXTRA_MAGIC_INCOMPATIBLE 27    // Incompatible with magic. Used for cyberware and bioware.
#define ITEM_EXTRA_KEPT               28    // Kept by the keep command.
#define ITEM_EXTRA_AIR_FILTRATION     29    // Provides the wearer with filtered air (gas mask, etc)
#define ITEM_EXTRA_NBC_RESISTANT      30    // Gives a bonus on resistance tests for contact chemical/toxin damage.
#define ITEM_EXTRA_NBC_IMMUNE         31    // Conveys immunity to contact chemical / toxin damage (full-body hazard suit, etc)
#define ITEM_EXTRA_PURGE_ON_DEATH     32    // Vanishes when you die (won't make it to your corpse)
#define MAX_ITEM_EXTRA                33

/* Ammo types */
#define AMMO_NORMAL     0
#define AMMO_APDS       1
#define AMMO_EXPLOSIVE  2
#define AMMO_EX         3
#define AMMO_FLECHETTE  4
#define AMMO_GEL        5
#define AMMO_HARMLESS   6
#define AMMO_AV         7
#define NUM_AMMOTYPES   8
// Adding a new ammo type? Make sure it's in npc_ammo_usage_preferences[].
// Make sure you also add a DB migration to include it in pfiles_ammo.
// Save and load it in bullet_pants.cpp's save_bullet_pants and load_bullet_pants functions.

/* material type for item */
#define ITEM_NONE                  0
#define ITEM_PLASTIC             1
#define ITEM_IRON                   2
#define ITEM_SILVER                 3

/* decking accessory types */

#define TYPE_FILE            0
#define TYPE_UPGRADE         1
#define TYPE_COMPUTER       2
#define TYPE_PARTS       3
#define TYPE_COOKER       4

/* vehicle types table */
#define VEH_DRONE         0
#define VEH_BIKE          1
#define VEH_CAR           2
#define VEH_TRUCK         3
#define VEH_FIXEDWING     4
#define VEH_ROTORCRAFT    5
#define VEH_VECTORTHRUST  6
#define VEH_HOVERCRAFT    7
#define VEH_MOTORBOAT     8
#define VEH_SHIP          9
#define VEH_LTA           10
#define VEH_SEMIBALLISTIC 11
#define VEH_SUBORBITAL    12
#define VEH_TRACKED       13
#define VEH_WALKER        14
#define NUM_VEH_TYPES     15
// Adding a type? Change constants.cpp's veh_types[]

/* vehicle affection table */
#define VAFF_NONE  0
#define VAFF_HAND  1
#define VAFF_SPD   2
#define VAFF_ACCL  3
#define VAFF_BOD  4
#define VAFF_ARM   5
#define VAFF_SIG  6
#define VAFF_AUTO  7
#define VAFF_SEAF  8
#define VAFF_SEAB       9
#define VAFF_LOAD  10
#define VAFF_SEN  11
#define VAFF_PILOT  12
#define VAFF_ULTRASONIC 13
#define VAFF_MAX  14
#define SHFT(ch) (GET_LEVEL(ch))

/* vehicle flag table */
#define VFLAG_NONE       0
#define VFLAG_CAN_FLY    1
#define VFLAG_AMPHIB     2
#define VFLAG_WORKSHOP   3
#define VFLAG_NEWBIE     4
#define VFLAG_ULTRASOUND 5
#define NUM_VFLAGS       6
/* vehicle speed table */
#define SPEED_OFF 0
#define SPEED_IDLE 1
#define SPEED_CRUISING 2
#define SPEED_SPEEDING 3
#define SPEED_MAX 4

/* cyberware */

#define BONE_PLASTIC  0
#define BONE_ALUMINIUM  1
#define BONE_TITANIUM  2
#define BONE_KEVLAR  3
#define BONE_CERAMIC  4

#define DATA_STANDARD  0
#define DATA_INDUCTION  1

#define EYE_CAMERA    (1 << 0)
#define EYE_CYBEREYES    (1 << 1)
#define EYE_DISPLAYLINK    (1 << 2)
#define EYE_FLARECOMP    (1 << 3)
#define EYE_IMAGELINK    (1 << 4)
#define EYE_LOWLIGHT    (1 << 5)
#define EYE_PROTECTIVECOVERS  (1 << 6)
#define EYE_CLOCK    (1 << 7)
#define EYE_RETINALDUPLICATION  (1 << 8)
#define EYE_THERMO    (1 << 9)
#define EYE_OPTMAG1    (1 << 10)
#define EYE_OPTMAG2    (1 << 11)
#define EYE_OPTMAG3    (1 << 12)
#define EYE_ELECMAG1    (1 << 13)
#define EYE_ELECMAG2    (1 << 14)
#define EYE_ELECMAG3    (1 << 15)
#define EYE_GUN      (1 << 16)
#define EYE_DATAJACK    (1 << 17)
#define EYE_DART    (1 << 18)
#define EYE_LASER    (1 << 19)
#define EYE_LIGHT    (1 << 20)
#define EYE_ULTRASOUND    (1 << 21)
#define EYE_COSMETIC    (1 << 22)
#define NUM_EYEMODS    23

#define ARMS_MOD_OBVIOUS    (1 << 0)
#define ARMS_MOD_SYNTHETIC    (1 << 1)
#define ARMS_MOD_ARMOR_MOD1    (1 << 2)
#define ARMS_MOD_STRENGTH_MOD1    (1 << 3)
#define ARMS_MOD_STRENGTH_MOD2    (1 << 4)
#define ARMS_MOD_STRENGTH_MOD3    (1 << 5)
#define ARMS_MOD_QUICKNESS_MOD1    (1 << 6)
#define ARMS_MOD_QUICKNESS_MOD2    (1 << 7)
#define ARMS_MOD_QUICKNESS_MOD3    (1 << 8)
#define ARMS_MOD_GYROMOUNT    (1 << 9)
#define NUM_ARMS_MODS    10

#define LEGS_MOD_OBVIOUS    (1 << 0)
#define LEGS_MOD_SYNTHETIC    (1 << 1)
#define LEGS_MOD_ARMOR_MOD1    (1 << 2)
#define LEGS_MOD_STRENGTH_MOD1    (1 << 3)
#define LEGS_MOD_STRENGTH_MOD2    (1 << 4)
#define LEGS_MOD_STRENGTH_MOD3    (1 << 5)
#define LEGS_MOD_QUICKNESS_MOD1    (1 << 6)
#define LEGS_MOD_QUICKNESS_MOD2    (1 << 7)
#define LEGS_MOD_QUICKNESS_MOD3    (1 << 8)
#define LEGS_MOD_KIDSTEALTH        (1 << 9)
#define NUM_LEGS_MODS    10

#define SKULL_MOD_OBVIOUS    (1 << 0)
#define SKULL_MOD_SYNTHETIC    (1 << 1)
#define SKULL_MOD_ARMOR_MOD1    (1 << 2)
#define NUM_SKULL_MODS    3

#define TORSO_MOD_OBVIOUS    (1 << 0)
#define TORSO_MOD_SYNTHETIC    (1 << 1)
#define TORSO_MOD_ARMOR_MOD1    (1 << 2)
#define TORSO_MOD_ARMOR_MOD2    (1 << 3)
#define TORSO_MOD_ARMOR_MOD3    (1 << 4)
#define NUM_TORSO_MODS    5

#define NERPS_WARE_VISIBLE   (1 << 0)
#define NUM_NERPS_MODS    1

#define FILTER_AIR  0
#define FILTER_BLOOD  1
#define FILTER_INGESTED  2

#define CYBERWEAPON_RETRACTABLE  0
#define CYBERWEAPON_IMPROVED  1

#define GRADE_STANDARD  0
#define GRADE_ALPHA  1
#define GRADE_BETA  2
#define GRADE_DELTA  3

#define CYB_CHIPJACK         0
#define CYB_DATAJACK         1
#define CYB_DATALOCK         2
#define CYB_KNOWSOFTLINK     3
#define CYB_MEMORY           4
#define CYB_TOOTHCOMPARTMENT 5
#define CYB_PHONE            6
#define CYB_RADIO            7
#define CYB_EYES             8
#define CYB_BONELACING       9
#define CYB_FINGERTIP        10
#define CYB_HANDBLADE        11
#define CYB_HANDRAZOR        12
#define CYB_HANDSPUR         13
#define CYB_VOICEMOD         14
#define CYB_BOOSTEDREFLEXES  15
#define CYB_DERMALPLATING    16
#define CYB_FILTRATION       17
#define CYB_SIMRIG           18
#define CYB_SKILLWIRE        19
#define CYB_VCR              20
#define CYB_WIREDREFLEXES    21
#define CYB_REACTIONENHANCE  22
#define CYB_REFLEXTRIGGER    23
#define CYB_BALANCEAUG       24
#define CYB_ORALDART         25
#define CYB_ORALGUN          26
#define CYB_ORALSLASHER      27
#define CYB_ASIST            28
#define CYB_CHIPJACKEXPERT   29
#define CYB_CRANIALCYBER     30
#define CYB_DATACOMPACT      31
#define CYB_ENCEPHALON       32
#define CYB_MATHSSPU         33
#define CYB_AUTOINJECT       34
#define CYB_BALANCETAIL      35
#define CYB_BODYCOMPART      36
#define CYB_FIN              37
#define CYB_SKULL            38
#define CYB_TORSO            39
#define CYB_DERMALSHEATHING  40
#define CYB_FOOTANCHOR       41
#define CYB_HYDRAULICJACK    42
#define CYB_MOVEBYWIRE       43
#define CYB_CLIMBINGCLAWS    44
#define CYB_SMARTLINK        45
#define CYB_MUSCLEREP        46
#define CYB_ARMS             47
#define CYB_LEGS             48
#define CYB_TACTICALCOMPUTER 49
#define CYB_CUSTOM_NERPS     50
#define CYB_CRD              51
#define CYB_FANGS            52
#define CYB_HORNS            53
#define NUM_CYBER            54

#define BIO_ADRENALPUMP    0
#define BIO_CATSEYES    1
#define BIO_DIGESTIVEEXPANSION  2
#define BIO_ENHANCEDARTIC  3
#define BIO_EXTENDEDVOLUME  4
#define BIO_METABOLICARRESTER  5
#define BIO_MUSCLEAUG    6
#define BIO_MUSCLETONER    7
#define BIO_NEPHRITICSCREEN  8
#define BIO_NICTATINGGLAND  9
#define BIO_ORTHOSKIN    10
#define BIO_PATHOGENICDEFENSE  11
#define BIO_PLATELETFACTORY  12
#define BIO_SUPRATHYROIDGLAND  13
#define BIO_SYMBIOTES    14
#define BIO_SYNTHACARDIUM  15
#define BIO_TAILOREDPHEROMONES  16
#define BIO_TOXINEXTRACTOR  17
#define BIO_TRACHEALFILTER  18
// Everything past this line is assumed to be cultured by default. If you add more that's not cultured, change the cultured check in db.cpp.
#define BIO_CEREBRALBOOSTER  19
#define BIO_DAMAGECOMPENSATOR  20
#define BIO_PAINEDITOR    21
#define BIO_REFLEXRECORDER  22
#define BIO_SYNAPTICACCELERATOR  23
#define BIO_THERMOSENSEORGAN  24
#define BIO_TRAUMADAMPER  25
#define BIO_CUSTOM_NERPS  26
#define NUM_BIOWARE       27

#define BIOWARE_STANDARD 0
#define BIOWARE_CULTURED 1

/* program types */
#define SOFT_BOD                1
#define SOFT_EVASION            2
#define SOFT_MASKING            3
#define SOFT_SENSOR             4
#define SOFT_ATTACK             5
#define SOFT_SLOW               6
#define SOFT_MEDIC              7
#define SOFT_SNOOPER    8
#define SOFT_BATTLETEC    9
#define SOFT_COMPRESSOR    10
#define SOFT_ANALYZE            11
#define SOFT_DECRYPT            12
#define SOFT_DECEPTION          13
#define SOFT_RELOCATE           14
#define SOFT_SLEAZE             15
#define SOFT_SCANNER    16
#define SOFT_BROWSE    17
#define SOFT_READ    18
#define SOFT_TRACK    19
#define SOFT_ARMOR    20
#define SOFT_CAMO    21
#define SOFT_CRASH    22
#define SOFT_DEFUSE    23
#define SOFT_EVALUATE    24
#define SOFT_VALIDATE    25
#define SOFT_SWERVE    26
#define SOFT_SUITE    27
#define SOFT_COMMLINK    28
#define SOFT_CLOAK    29
#define SOFT_LOCKON    30
#define SOFT_ASIST_COLD    31
#define SOFT_ASIST_HOT    32
#define SOFT_HARDENING    33
#define SOFT_ICCM    34
#define SOFT_ICON    35
#define SOFT_MPCP    36
#define SOFT_REALITY    37
#define SOFT_RESPONSE    38
#define NUM_PROGRAMS    39

#define PART_ACTIVE    1
#define PART_STORAGE    2
#define PART_ASIST_HOT    3
#define PART_ASIST_COLD    4
#define PART_HARDENING    5
#define PART_ICCM    6
#define PART_ICON    7
#define PART_IO      8
#define PART_MPCP    9
#define PART_BOD    10
#define PART_SENSOR    11
#define PART_MASKING    12
#define PART_EVASION    13
#define PART_PORTS    14
#define PART_RAS_OVERRIDE  15
#define PART_REALITY_FILTER  16
#define PART_RESPONSE    17
#define PART_MATRIX_INTERFACE  18
#define PART_MASER    19
#define PART_CELLULAR    20
#define PART_LASER    21
#define PART_MICROWAVE    22
#define PART_RADIO    23
#define PART_SATELLITE    24
#define PART_SIGNAL_AMP    25
#define NUM_PARTS    26

#define TYPE_KIT        1
#define TYPE_WORKSHOP   2
#define TYPE_FACILITY   3

// Kit/Workshop/Facility types.
#define TYPE_GENERAL       0
#define TYPE_ELECTRONIC    1
#define TYPE_MICROTRONIC   2
#define TYPE_CYBERWARE     3
#define TYPE_VEHICLE       4
#define TYPE_WEAPON        5
#define TYPE_MEDICAL       6
#define TYPE_AMMO          7
#define TYPE_GUNSMITHING   8
#define NUM_WORKSHOP_TYPES 9
/* modifier constants used with obj affects ('A' fields) */

#define APPLY_NONE               0      /* No effect                    */
#define APPLY_BOD                1      /* Apply to Body                */
#define APPLY_QUI                2      /* Apply to Quickness           */
#define APPLY_STR                3      /* Apply to Strength            */
#define APPLY_CHA                4      /* Apply to Charisma            */
#define APPLY_INT                5      /* Apply to Intelligence        */
#define APPLY_WIL                6      /* Apply to Willpower           */
#define APPLY_ESS                7      /* Apply to Essence             */
#define APPLY_MAG                8      /* Apply to Magic               */
#define APPLY_REA                9      /* Apply to Reaction            */
#define APPLY_AGE               10      /* Apply to age                 */
#define APPLY_CHAR_WEIGHT       11      /* Apply to weight              */
#define APPLY_CHAR_HEIGHT       12      /* Apply to height              */
#define APPLY_MENTAL            13      /* Apply to max mental          */
#define APPLY_PHYSICAL          14      /* Apply to max physical points */
#define APPLY_MOVE              15      /* Apply to max move points     */
#define APPLY_BALLISTIC         16      /* Apply to Ballistic armor     */
#define APPLY_IMPACT            17      /* Apply to Impact armor rating */
#define APPLY_ASTRAL_POOL       18      /* Apply to Astral Pool         */
#define APPLY_DEFENSE_POOL      19      /* Apply to Defense Pool        */
#define APPLY_COMBAT_POOL       20      /* Apply to Dodge Pool          */
#define APPLY_HACKING_POOL      21      /* Apply to Hacking Pool        */
#define APPLY_MAGIC_POOL        22      /* Apply to Magic Pool          */
#define APPLY_INITIATIVE_DICE   23      /* Apply to Initiative Dice     */
#define APPLY_TARGET            24      /* Apply to Target Numbers      */
#define APPLY_CONTROL_POOL      25
#define APPLY_TASK_BOD    26
#define APPLY_TASK_QUI    27
#define APPLY_TASK_STR    28
#define APPLY_TASK_CHA    29
#define APPLY_TASK_INT    30
#define APPLY_TASK_WIL    31
#define APPLY_TASK_REA    32
#define APPLY_MAX               33

/* container flags - value[1] */

#define CONT_CLOSEABLE      (1 << 0)    /* Container can be closed      */
#define CONT_PICKPROOF      (1 << 1)    /* Container is pickproof       */
#define CONT_CLOSED         (1 << 2)    /* Container is closed          */
#define CONT_LOCKED         (1 << 3)    /* Container is locked          */

/* some different kind of liquids for use in values of drink containers */
/* New and improved liquids by Jordan */

#define LIQ_WATER      0
#define LIQ_SODA       1
#define LIQ_COFFEE     2
#define LIQ_MILK       3
#define LIQ_JUICE      4
#define LIQ_TEA        5
#define LIQ_SOYKAF     6
#define LIQ_SMOOTHIE   7
#define LIQ_SPORTS     8
#define LIQ_ESPRESSO   9
#define LIQ_BEER       10
#define LIQ_ALE        11
#define LIQ_LAGER      12
#define LIQ_IMPORTBEER 13
#define LIQ_MICROBREW  14
#define LIQ_MALTLIQ    15
#define LIQ_COCKTAIL   16
#define LIQ_MARGARITA  17
#define LIQ_LIQUOR     18
#define LIQ_EVERCLEAR  19
#define LIQ_VODKA      20
#define LIQ_TEQUILA    21
#define LIQ_BRANDY     22
#define LIQ_RUM        23
#define LIQ_WHISKEY    24
#define LIQ_GIN        25
#define LIQ_CHAMPAGNE  26
#define LIQ_REDWINE    27
#define LIQ_WHITEWINE  28
#define LIQ_BLOOD      29
#define LIQ_HOTSAUCE   30
#define LIQ_PISS       31
#define LIQ_LOCAL      32
#define LIQ_FUCKUP     33
#define LIQ_CLEANER    34
#define LIQ_HOT_COCOA  35
#define LIQ_COOLANT    36
#define LIQ_MOTOR_OIL  37
#define LIQ_BATTERY_ACID 38
/* Adding a liquid type? You must modify FOUR things in constants.cpp:
   - color_liquid
   - drinks
   - drinknames
   - drink_aff
*/

#define NUM_DRINK_TYPES         39

/* focus values */

#define VALUE_FOCUS_TYPE        0
#define VALUE_FOCUS_RATING      1
#define VALUE_FOCUS_CAT         2
#define VALUE_FOCUS_BONDED      5
/* #define VALUE_FOCUS_WEAPON_VNUM      7 */
#define VALUE_FOCUS_SPECIFY     8
#define VALUE_FOCUS_CHAR_IDNUM  9

/* types of foci */

#define FOCI_EXPENDABLE    0
#define FOCI_SPEC_SPELL    1
#define FOCI_SPELL_CAT    2
#define FOCI_SPIRIT    3
#define FOCI_POWER    4
#define FOCI_SUSTAINED    5
// #define FOCI_WEAPON    6
#define FOCI_SPELL_DEFENSE 7

#define ACCESS_SMARTLINK  1
#define ACCESS_SCOPE    2
#define ACCESS_GASVENT    3
#define ACCESS_SHOCKPAD    4
#define ACCESS_SILENCER    5
#define ACCESS_SOUNDSUPP  6
#define ACCESS_SMARTGOGGLE  7
#define ACCESS_BIPOD    8
#define ACCESS_TRIPOD    9
#define ACCESS_BAYONET    10

#define ACCESS_LOCATION_TOP    7
#define ACCESS_LOCATION_BARREL 8
#define ACCESS_LOCATION_UNDER  9

// Accessory value 0
#define ACCESS_ACCESSORY_LOCATION_TOP    0
#define ACCESS_ACCESSORY_LOCATION_BARREL 1
#define ACCESS_ACCESSORY_LOCATION_UNDER  2

#define ACCESS_ACCESSORY_LOCATION_DELTA (ACCESS_LOCATION_TOP - ACCESS_ACCESSORY_LOCATION_TOP)

#define RECOIL_COMP_VALUE_BIPOD   2
#define RECOIL_COMP_VALUE_TRIPOD  6

#define MOD_NOWHERE    0
#define MOD_INTAKE_FRONT  1
#define MOD_INTAKE_MID    2
#define MOD_INTAKE_REAR    3
#define MOD_ENGINE    4
#define MOD_EXHAUST    5
#define MOD_COOLING    6
#define MOD_BRAKES    7
#define MOD_SUSPENSION    8
#define MOD_WHEELS    9
#define MOD_TIRES    10
#define MOD_BODY_FRONT    11
#define MOD_BODY_DRIVERS  12
#define MOD_BODY_PASS    13
#define MOD_BODY_REAR    14
#define MOD_BODY_HOOD    15
#define MOD_BODY_ROOF    16
#define MOD_BODY_SPOILER  17
#define MOD_BODY_WINDSCREEN  18
#define MOD_BODY_WINDOWS  19
#define MOD_ARMOR    20
#define MOD_SEAT    21
#define MOD_COMPUTER1    22
#define MOD_COMPUTER2    23
#define MOD_COMPUTER3    24
#define MOD_PHONE    25
#define MOD_RADIO    26
#define MOD_MOUNT    27
#define NUM_MODS    28

#define TYPE_ENGINECUST    1
#define TYPE_NOS    2
#define TYPE_TURBOCHARGER  3
#define TYPE_AUTONAV    4
#define TYPE_CMC    5
#define TYPE_DRIVEBYWIRE  6
#define TYPE_IMPSUSP    7
#define TYPE_MOTORBIKEGYRO  8
#define TYPE_OFFROADSUS    9
#define TYPE_ARMOR    10
#define TYPE_CONCEALEDARMOR  11
#define TYPE_CRASHCAGE    12
#define TYPE_ROLLBARS    13
#define TYPE_THERMALMASK  14
#define TYPE_MOUNT    15
#define TYPE_ECM    16
#define TYPE_ECCM    17
#define TYPE_SENSORS    18
#define TYPE_SEATS    19
#define TYPE_TIRES    20
#define TYPE_MISC    21
#define TYPE_AMMOBIN    22
#define TYPE_POKEYSTICK 23
#define NUM_MODTYPES    24

#define ENGINE_NONE        0
#define ENGINE_ELECTRIC    1
#define ENGINE_FUELCELL    2
#define ENGINE_GASOLINE    3
#define ENGINE_METHANE    4
#define ENGINE_DIESEL      5
#define NUM_ENGINE_TYPES  6

#define SEATING_REAR      0
#define SEATING_FRONT     1

/* house value defines */
#define MAX_HOUSES      100
#define MAX_GUESTS      10
#define NORMAL_MAX_GUESTS    2
#define OBJS_PER_LIFESTYLE   35

#define HOUSE_LOW     0
#define HOUSE_MIDDLE  1
#define HOUSE_HIGH    2
#define HOUSE_LUXURY  3

#define MODE_SS    1
#define MODE_SA    2
#define MODE_BF    3
#define MODE_FA    4

/* alias defines */
#define ALIAS_SIMPLE    0
#define ALIAS_COMPLEX   1

#define ALIAS_SEP_CHAR  ';'
#define ALIAS_VAR_CHAR  '$'
#define ALIAS_GLOB_CHAR '*'

/* Subcommands section: Originally from interpreter.h */

/* do_cast */
#define SCMD_STANDARD_CAST  0
#define SCMD_RITUAL_CAST    1

/* do_exits */
#define SCMD_LONGEXITS  0
#define SCMD_SHORTEXITS 1

/* do_dig */
#define SCMD_DIG        0
#define SCMD_UNDIG      1

/* directions */
#define SCMD_NORTH      1
#define SCMD_NORTHEAST  2
#define SCMD_EAST       3
#define SCMD_SOUTHEAST  4
#define SCMD_SOUTH      5
#define SCMD_SOUTHWEST  6
#define SCMD_WEST       7
#define SCMD_NORTHWEST  8
#define SCMD_UP         9
#define SCMD_DOWN      10

/* do_gen_ps */
#define SCMD_INFO       0
#define SCMD_HANDBOOK   1
#define SCMD_CREDITS    2
#define SCMD_NEWS       3
#define SCMD_POLICIES   4
#define SCMD_VERSION    5
#define SCMD_IMMLIST    6
#define SCMD_MOTD       7
#define SCMD_IMOTD      8
#define SCMD_CLEAR      9
#define SCMD_WHOAMI     10

/* do_wizutil */
#define SCMD_PARDON       0
#define SCMD_NOTITLE      1
#define SCMD_SQUELCH      2
#define SCMD_FREEZE       3
#define SCMD_THAW         4
#define SCMD_UNAFFECT     5
#define SCMD_SQUELCHOOC   6
#define SCMD_INITIATE     7
#define SCMD_RPE          8
#define SCMD_POWERPOINT   9
#define SCMD_AUTHORIZE    10
#define SCMD_SQUELCHTELLS 11
#define SCMD_MUTE_NEWBIE  12

/* do_say */
#define SCMD_SAY        0
#define SCMD_OSAY       1
#define SCMD_SAYTO      2

/* do_spec_com */
#define SCMD_WHISPER    0
#define SCMD_ASK        1

/* do_gen_com */
#define SCMD_SHOUT      0
#define SCMD_NEWBIE     1
#define SCMD_OOC        2
#define SCMD_RPETALK 3
#define SCMD_HIREDTALK 4

/* do_last */
#define SCMD_LAST     0
#define SCMD_FINGER   1

/* do_shutdown */
#define SCMD_SHUTDOW    0
#define SCMD_SHUTDOWN   1

/* do_quit */
#define SCMD_QUI        0
#define SCMD_QUIT       1

/* do_date */
#define SCMD_DATE       0
#define SCMD_UPTIME     1

/* do disconnect */
#define SCMD_DISCONNECT    0
#define SCMD_MORTED        1

/* do wiztitle */
#define SCMD_WHOTITLE      0
#define SCMD_PRETITLE      1

/* do_commands */
#define SCMD_COMMANDS   0
#define SCMD_SOCIALS    1

/* do_drop */
#define SCMD_DROP       0
#define SCMD_JUNK       1
#define SCMD_DONATE     2

/* do_gen_write */
#define SCMD_BUG        0
#define SCMD_TYPO       1
#define SCMD_IDEA       2
#define SCMD_PRAISE     3

/* do_look */
#define SCMD_LOOK       0
#define SCMD_READ       1
#define SCMD_QUICKLOOK  2

/* do_examine */
#define SCMD_EXAMINE    0
#define SCMD_PROBE      1

/* do_qcomm */
#define SCMD_QSAY       0
#define SCMD_QECHO      1

/* do_pour */
#define SCMD_POUR       0
#define SCMD_FILL       1

/* do_poof */
#define SCMD_POOFIN     0
#define SCMD_POOFOUT    1

/* do_astral */
#define SCMD_PROJECT       0
#define SCMD_PERCEIVE      1

/* do_hit */
#define SCMD_HIT        0
#define SCMD_MURDER     1
#define SCMD_KILL       2

/* do_eat */
#define SCMD_EAT        0
#define SCMD_TASTE      1
#define SCMD_DRINK      2
#define SCMD_SIP        3

/* do_use */
#define SCMD_USE        0

/* do_new_emote */
#define SCMD_ECHO       0
#define SCMD_EMOTE      1
#define SCMD_VEMOTE     2
#define SCMD_AECHO      3

/* do_gen_door */
#define SCMD_OPEN       0
#define SCMD_CLOSE      1
#define SCMD_UNLOCK     2
#define SCMD_LOCK       3
#define SCMD_PICK       4
#define SCMD_KNOCK      5

/* matrix subcommands */
#define SCMD_INSTALL    1
#define SCMD_UNINSTALL  2

/* do_skills */
#define SCMD_SKILLS     0
#define SCMD_ABILITIES  1

/* do_time */
#define SCMD_NORMAL     0
#define SCMD_PRECISE    1

/* do_subscribe */
#define SCMD_SUB 0
#define SCMD_UNSUB 1

/* do_phone */
#define SCMD_RING 1
#define SCMD_HANGUP 2
#define SCMD_TALK 3
#define SCMD_ANSWER 4

/* do_man */
#define SCMD_UNMAN 1

/* do_load */
#define SCMD_SWAP   0
#define SCMD_UPLOAD   1
#define SCMD_UNLOAD  2

#define SCMD_DRAIN  0
#define SCMD_REFLECT  1
#define SCMD_SDEFENSE  2

/* do_practice */
#define SCMD_UNPRACTICE 1

/* do_train */
#define SCMD_UNTRAIN 1

/* do_crash_mud */
#define SCMD_NOOP 0 // AKA 'this invocation does nothing'.
#define SCMD_BOOM 1337

/* do_get */
#define SCMD_TAKE 1

/* END SUBCOMMANDS SECTION */

#define FREE    0
#define SIMPLE    1
#define COMPLEX    2
#define EXCLUSIVE  3
#define NOCOMBAT  4

/* modes of connectedness: used by descriptor_data.state */

#define CON_PLAYING      0              /* Playing - Nominal state      */
#define CON_CLOSE        1              /* Disconnecting                */
#define CON_GET_NAME     2              /* By what name ..?             */
#define CON_NAME_CNFRM   3              /* Did I get that right, x?     */
#define CON_PASSWORD     4              /* Password:                    */
#define CON_NEWPASSWD    5              /* Give me a password for x     */
#define CON_CNFPASSWD    6              /* Please retype password:      */
#define CON_CCREATE      7
#define CON_RMOTD        8              /* PRESS RETURN after MOTD      */
#define CON_MENU         9              /* Your choice: (main menu)     */
#define CON_PART_CREATE  10
#define CON_CHPWD_GETOLD 11             /* Changing passwd: get old     */
#define CON_CHPWD_GETNEW 12             /* Changing passwd: get new     */
#define CON_CHPWD_VRFY   13             /* Verify new password          */
#define CON_DELCNF1      14             /* Delete confirmation 1        */
#define CON_DELCNF2      15             /* Delete confirmation 2        */
#define CON_QMENU        16             /* quit menu                    */
#define CON_QGETOLDPW    17
#define CON_QGETNEWPW    18
#define CON_QVERIFYPW    19
#define CON_QDELCONF1    20
#define CON_QDELCONF2    21
#define CON_SPELL_CREATE 22             /* Spell creation menus         */
#define CON_PCUSTOMIZE   23             /* customize persona description menu */
#define CON_ACUSTOMIZE   24             /* customize reflection description menu */
#define CON_FCUSTOMIZE   25
#define CON_VEDIT        26
#define CON_IEDIT        27  /* olc edit mode */
#define CON_REDIT        28  /* olc edit mode */
#define CON_MEDIT        29
#define CON_QEDIT        30
#define CON_SHEDIT       31
#define CON_ZEDIT        32
#define CON_HEDIT        33
#define CON_ICEDIT       34
#define CON_PRO_CREATE   35
#define CON_DECK_CREATE  36
#define CON_SPE_CREATE   37
#define CON_INITIATE     38
#define CON_DECORATE     39
#define CON_POCKETSEC    40
#define CON_VEHCUST      41
#define CON_BCUSTOMIZE   42
#define CON_TRIDEO       43
#define CON_AMMO_CREATE  44
#define CON_ASKNAME      45            /* Ask user for name            */
#define CON_PGEDIT       46
#define CON_HELPEDIT     47
// If you add another state, you need to touch comm.cpp's close_socket and make sure it's reflected there!

/* arbitrary constants used by index_boot() (must be unique) */
#define DB_BOOT_WLD     0
#define DB_BOOT_MOB     1
#define DB_BOOT_OBJ     2
#define DB_BOOT_ZON     3
#define DB_BOOT_SHP     4
#define DB_BOOT_QST     5
#define DB_BOOT_VEH     6
#define DB_BOOT_MTX     7
#define DB_BOOT_IC      8
/* Defines for sending text */

#define TO_ROOM                1
#define TO_VICT                2
#define TO_NOTVICT             3
#define TO_CHAR                4
#define TO_ROLLS               5
#define TO_VEH                 6
#define TO_DECK                 7
#define TO_VEH_ROOM            8
#define TO_CHAR_INCLUDE_RIGGER 9
#define TO_CHAR_FORCE          10
#define TO_VICT_INCLUDE_RIGGER 11
#define TO_VICT_FORCE          12
#define TO_SLEEP               (1 << 5)     /* to char, even if sleeping */
#define TO_REMOTE              (1 << 6)

/* Boards */

#ifdef USE_PRIVATE_CE_WORLD
#define NUM_OF_BOARDS           3
#else
#define NUM_OF_BOARDS           2 /* change if needed! */
#endif

#define MAX_BOARD_MESSAGES      300     /* arbitrary -- change if needed */
#define MAX_MESSAGE_LENGTH      5120    /* arbitrary -- change if needed */
#define INDEX_SIZE         ((NUM_OF_BOARDS*MAX_BOARD_MESSAGES) + 5)

/* teacher modes */

#define LIBRARY          0
#define NEWBIE          1
#define AMATEUR         2
#define ADVANCED        3
#define GODLY           4

/* sun state for weather_data */

#define SUN_DARK        0
#define SUN_RISE        1
#define SUN_LIGHT       2
#define SUN_SET         3

#define MOON_NEW        0
#define MOON_WAX        1
#define MOON_FULL       2
#define MOON_WANE       3

/* sky conditions for weather_data */

#define SKY_CLOUDLESS   0
#define SKY_CLOUDY      1
#define SKY_RAINING     2
#define SKY_LIGHTNING   3

/* rent codes */

#define RENT_UNDEF      0
#define RENT_CRASH      1
#define RENT_RENTED     2
#define RENT_CRYO       3
#define RENT_FORCED     4
#define RENT_TIMEDOUT   5

/* MATRIX HOSTS TYPES */
#define HOST_DATASTORE  0
#define HOST_LTG        1
#define HOST_RTG        2
#define HOST_PLTG        3
#define HOST_SECURITY    4
#define HOST_CONTOLLER  5

/* IC Options */
#define IC_ARMOR        1
#define IC_CASCADE      2
#define IC_EX_DEFENSE    3
#define IC_EX_OFFENSE    4
#define IC_SHIELD        5
#define IC_SHIFT        6
#define IC_TRAP          7

#define ZONE_SEATTLE    0
#define ZONE_PORTLAND    1
#define ZONE_CARIB      2
#define ZONE_OCEAN      3

/* MOB Alert Levels */
#define MALERT_CALM      0
#define MALERT_ALERT    1
#define MALERT_ALARM    2

#define LIGHT           1
#define MODERATE        2
#define SERIOUS         3
#define DEADLY          4

#define QUEST_TIMER  15

#define OPT_USEC                  100000  /* 10 passes per second */
#define PASSES_PER_SEC            (1000000 / OPT_USEC)
#define RL_SEC                    * PASSES_PER_SEC

#define PULSE_ZONE                (3 RL_SEC)
#define PULSE_SPECIAL             (10 RL_SEC)
#define PULSE_MOBILE              (10 RL_SEC)
#define PULSE_VIOLENCE            (3 RL_SEC)
#define PULSE_MONORAIL            (5 RL_SEC)

#ifdef USE_PRIVATE_CE_WORLD
#define MAX_SOCK_BUF              4194304 /* hand-picked from `cat /proc/sys/net/ipv4/tcp_wmem` */
#else
// You can probably change this value (see example above)-- it was set in the dark ages where the max sock buf was 12288.
#define MAX_SOCK_BUF              (48 * 1024) /* Size of kernel's sock buf   */
#endif

#define MAX_PROMPT_LENGTH         1000          /* Max length of prompt        */
#define GARBAGE_SPACE             32
#define SMALL_BUFSIZE             1024
#define LARGE_BUFSIZE             (MAX_SOCK_BUF - GARBAGE_SPACE - MAX_PROMPT_LENGTH)

#define MAX_STRING_LENGTH         32768
#define MAX_INPUT_LENGTH          2048     /* Max length per *line* of input */
#define MAX_RAW_INPUT_LENGTH      4096     /* Max size of *raw* input */
#define MAX_MESSAGES              100
#define MAX_NAME_LENGTH           20  /* Used in char_file_u *DO*NOT*CHANGE* */ // if you change this, update the fscanf in ban.cpp and comm.cpp
#define MAX_RESTRING_LENGTH       256

#define MAX_PWD_LENGTH            30  /* Relic of the past, do not change. Dictates max length of crypt() hashes. */
#define MAX_TITLE_LENGTH          48  /* Used in char_file_u *DO*NOT*CHANGE* */
#define MAX_WHOTITLE_LENGTH       10  /* Used in char_file_u *DO*NOT*CHANGE* */
#define HOST_LENGTH               500
#define LINE_LENGTH               80  /* Used in char_file_u *DO*NOT*CHANGE* */
#define EXDSCR_LENGTH             2040/* Used in char_file_u *DO*NOT*CHANGE* */
#define MAX_AFFECT                32  /* Used in char_file_u *DO*NOT*CHANGE* */
#define MAX_OBJ_AFFECT            6 /* Used in obj_file_elem *DO*NOT*CHANGE* */
#define IDENT_LENGTH              8

// Update the DB if you change these.
#define MAX_KEYWORDS_LEN          200
#define MAX_MOVEMENT_LEN          200
#define MAX_SHORTDESC_LEN         200

// New combat modifiers used in the rework of hit().
// Adding an additional one? Make sure you list it in the dodge check switch case if appropriate.
#define COMBAT_MOD_RECOIL                0
#define COMBAT_MOD_MOVEMENT              1
#define COMBAT_MOD_DUAL_WIELDING         2
#define COMBAT_MOD_SMARTLINK             3
#define COMBAT_MOD_DISTANCE              4
#define COMBAT_MOD_VISIBILITY            5
#define COMBAT_MOD_POSITION              6
#define COMBAT_MOD_GYRO                  7
#define COMBAT_MOD_REACH                 8
#define COMBAT_MOD_VEHICLE_DAMAGE        9
#define COMBAT_MOD_DEFENDER_MOVING       10
#define COMBAT_MOD_IN_MELEE_COMBAT       11
#define COMBAT_MOD_OPPONENT_BURST_COUNT  12
#define COMBAT_MOD_FOOTANCHORS           13
#define COMBAT_MOD_VEHICLE_HANDLING      14
#define NUM_COMBAT_MODIFIERS             15
// End new combat modifiers.

#define JURISDICTION_SEATTLE   0
#define JURISDICTION_PORTLAND  1
#define JURISDICTION_CARIBBEAN 2
#define JURISDICTION_OCEAN     3

// Locations, to remove the magic numbers from the code.
#define RM_CHARGEN_START_ROOM        60500
#define RM_NEWBIE_LOADROOM           60565 // The Neophyte Hotel.
#define RM_NEWBIE_LOBBY              60563
#define RM_ENTRANCE_TO_DANTES        35500
#define RM_DANTES_GARAGE             35693
#define RM_DANTES_GARAGE_RANDOM      (35693 + number(0,4))
#define RM_DANTES_DESCENT            35502
#define RM_AGGIES_DRUG_STORE         14649
#define RM_DETENTION_CELL_A          3726
#define RM_HEARL_STREET              9915
#define RM_ALONG_THIRD_STREET        62267
#define RM_MASSAGE_PARLOR            7226
#define RM_ABANDONED_CLOTHING_STORE  4985
#define RM_CAIROS_APARTMENT          96100
#ifdef USE_PRIVATE_CE_WORLD
#define RM_TEMPORARY_DOCWAGON        10097
#define RM_SEATTLE_DOCWAGON          RM_TEMPORARY_DOCWAGON
#define RM_PORTLAND_DOCWAGON         RM_TEMPORARY_DOCWAGON
#define RM_CARIB_DOCWAGON            RM_TEMPORARY_DOCWAGON
#define RM_OCEAN_DOCWAGON            RM_TEMPORARY_DOCWAGON
#define RM_NERPCORPOLIS_RECEPTIONIST 6900
#define RM_NERPCORPOLIS_LOBBY        6901
#define RM_PORTLAND_PARKING_GARAGE1  2751 + number(0, 2)
#define RM_PORTLAND_PARKING_GARAGE2  2756 + number(0, 2)
#define RM_PORTLAND_PARKING_GARAGE3  2762 + number(0, 2)
#else
#define RM_SEATTLE_DOCWAGON          RM_ENTRANCE_TO_DANTES
#define RM_PORTLAND_DOCWAGON         RM_ENTRANCE_TO_DANTES
#define RM_CARIB_DOCWAGON            RM_ENTRANCE_TO_DANTES
#define RM_OCEAN_DOCWAGON            RM_ENTRANCE_TO_DANTES
#define RM_NERPCORPOLIS_RECEPTIONIST RM_ENTRANCE_TO_DANTES
#define RM_NERPCORPOLIS_LOBBY        RM_ENTRANCE_TO_DANTES
#define RM_PORTLAND_PARKING_GARAGE   RM_DANTES_GARAGE
#endif
#define RM_SEATTLE_PARKING_GARAGE    RM_DANTES_GARAGE
#define RM_CARIB_PARKING_GARAGE      RM_DANTES_GARAGE
#define RM_OCEAN_PARKING_GARAGE      RM_DANTES_GARAGE
#define RM_PAINTER_LOT               37518
#define RM_MULTNOMAH_GATE_NORTH      17598
#define RM_MULTNOMAH_GATE_SOUTH      17599
#define RM_MAGE_TRAINER              778
#define RM_GUN_TRAINER               781
#define RM_PORTABLE_VEHICLE_STORAGE  10097
#define RM_MORTAL_RELATIONS_CONFERENCE_ROOM 10044

// Chargen room defines
#define RM_CHARGEN_PATH_OF_THE_MAGICIAN_SHAMANIC  60520
#define RM_CHARGEN_PATH_OF_THE_MAGICIAN_HERMETIC  60623
#define RM_CHARGEN_PATH_OF_THE_ADEPT              60518
#define RM_CHARGEN_CONJURING_HERMETIC             60522
#define RM_CHARGEN_CONJURING_SHAMANIC             60625

// Archetypal chargen room defines
#define RM_ARCHETYPAL_CHARGEN_PATH_OF_THE_MAGICIAN_HERMETIC 90701
#define RM_ARCHETYPAL_CHARGEN_PATH_OF_THE_MAGICIAN_SHAMANIC 90705
#define RM_ARCHETYPAL_CHARGEN_CONJURING_HERMETIC 90704
#define RM_ARCHETYPAL_CHARGEN_CONJURING_SHAMANIC 90708

#define RM_JUNKYARD_GATES           70501
#define RM_JUNKYARD_PARTS           70502
#define RM_JUNKYARD_GLASS           70504
#define RM_JUNKYARD_APPLI           70505
#define RM_JUNKYARD_ELECT           70509
#define NUM_JUNKYARD_ROOMS          5

// This is specifically not included in the above since not every vehicle can leave it successfully. BIKES AND DRONES ONLY.
#define RM_JUNKYARD_BIKES           70508

// Objects, to remove the magic numbers from the code.
#define OBJ_NEWBIE_RADIO                   60531
#define OBJ_MULTNOMAH_VISA                 17513
#define OBJ_CARIBBEAN_VISA                 62100
#define OBJ_MAP_OF_SEATTLE                 2041
#define OBJ_ELEVATOR_SHAFT_KEY             998
#define OBJ_SEATTLE_TAXI_SIGN              600
#define OBJ_PORTLAND_TAXI_SIGN             699
#define OBJ_CARIBBEAN_TAXI_SIGN            610
#define OBJ_SPECIAL_PC_CORPSE              43
#define OBJ_COLT_M23                       838
#define OBJ_NICAMI_SCOPE                   28702
#define OBJ_SMARTLINK_II                   31111
#define OBJ_DOCWAGON_BASIC_MOD             16206
#define OBJ_DOCWAGON_GOLD_MODULATOR        16207
#define OBJ_DOCWAGON_PLATINUM_MODULATOR    16208
#define OBJ_BLACK_TRENCH_COAT              1833
#define OBJ_100_PCT_COTTON_SHIRT           64903
#define OBJ_FOREARM_GUARDS                 700
#define OBJ_RIFLE_STRAP                    2059
#define OBJ_BLACK_LEATHER_DUTY_BELT        9402
#define OBJ_BLACK_BDU_PANTS                14618
#define OBJ_BLACK_COMBAT_BOOTS             29010
#define OBJ_MEDKIT                         450
#define OBJ_VEHICLE_TOOLKIT                37597
#define OBJ_ELECTRONICS_KIT                60532
#define OBJ_CELL_PHONE                     60530
#define OBJ_POCKET_SECRETARY               31542
#define OBJ_NEOPHYTE_DUFFELBAG             60535
#define OBJ_POLEARM_STRAP                  64989
#define OBJ_THERMOGRAPHIC_GOGGLES          378
#define OBJ_STEEL_COMBAT_AXE               658
#define OBJ_SECURE_JACKET                  681
#define OBJ_TOP_LASER_SIGHT                626
#define OBJ_BAYONET                        749
#define OBJ_LONDON_FOG_MERLIN              8438
#define OBJ_PLATED_ARMOR_VEST              707
#define OBJ_BLACK_SLACKS                   8455
#define OBJ_BLACK_DRESS_SHOES              38005
#define OBJ_LONDON_FOG_COUNT               8435
#define OBJ_ASH_LEAF_ANKLET                60565
#define OBJ_ORICHALCUM_BRACELET            4906
#define OBJ_SCK_MODEL_100                  91000
#define OBJ_FOREGRIP                       28618
#define OBJ_PAIR_OF_WHITE_TRAINERS         22691
#define OBJ_CYBERDECK_REPAIR_KIT           38041
#define OBJ_NOVATECH_BURNER                1172
#define OBJ_MITSUHAMA_Z4                   1847
#define OBJ_LONDON_FOG_PROFESSIONAL        8439
#define OBJ_THIGH_HOLSTER                  3494
#define OBJ_MARKSMAN_LETTER                5962
#define OBJ_MARKSMAN_BADGE                 5961
#define OBJ_MAGEBLING                      5734
#define OBJ_MAGE_LETTER                    5735
#define OBJ_MODIFIED_BARRET_SNIPER_RIFLE   14636
#define OBJ_STAFF_REBATE_FOR_DECKBUILDING  10003
#define OBJ_EYEBALL_KEY                    22892
#define OBJ_ELECTRONIC_EYEBALL_KEY         15889
#define OBJ_ENCYCLOPEDIA_LABELED_O         38095
#define OBJ_FIBEROPTIC_CRYSTAL             35029
#define OBJ_OPTICAL_CHIP_KEY               4600
#define OBJ_UNFINISHED_EQUATION            1015
#define OBJ_SCANEYE                        3817

#define OBJ_TITLE_TO_AMERICAR              891
#define OBJ_TITLE_TO_SCORPION              892
#define OBJ_TITLE_TO_JACKRABBIT            893
#define OBJ_TITLE_TO_RUNABOUT              894
#define OBJ_TITLE_TO_RAPIER                895
#define OBJ_TITLE_TO_BISON                 897
#define OBJ_TITLE_TO_WESTWIND              898
#define OBJ_TITLE_TO_DOBERMAN              904
#define OBJ_TITLE_TO_SNOOPER               905
#define OBJ_TITLE_TO_SURVEILLANCE          906
#define OBJ_TITLE_TO_ROTODRONE             907
#define OBJ_TITLE_TO_DALMATION             908
#define OBJ_TITLE_TO_SUPERCOMBI_RV         961
#define OBJ_TITLE_TO_NOMAD_SUV             962
#define OBJ_TITLE_TO_BRUMBY_SUV            963
#define OBJ_TITLE_TO_GOPHER_PICKUP         964
#define OBJ_TITLE_TO_TRANSPORT_PICKUP      965
#define OBJ_TITLE_TO_GMC_4201              966
#define OBJ_TITLE_TO_GMC_BULLDOG           967
#define OBJ_TITLE_TO_ARES_ROADMASTER       968
#define OBJ_TITLE_TO_WHITE_EAGLE_BIKE      969

#ifdef USE_PRIVATE_CE_WORLD
#define OBJ_GAS_VENT_IV                    80403
#define OBJ_CMT_AVATAR                     80004
#define OBJ_TOP_MOUNTED_SMARTGUN_LINK      80422
#define OBJ_FORMFIT_III                    80734
#else
#define OBJ_GAS_VENT_IV                    691
#define OBJ_CMT_AVATAR                     29005
#define OBJ_TOP_MOUNTED_SMARTGUN_LINK      60554
#define OBJ_FORMFIT_III                    711
#endif


#define OBJ_NOVATECH_SIX_SENSORS           17116
#define OBJ_NOVATECH_SIX_MASKING           17115
#define OBJ_NOVATECH_SIX_BOD               17113
#define OBJ_FUCHI_LTD_EVASION              6105
#define OBJ_CATCO_SLEAZE                   6108
#define OBJ_TRANSYS_SCRIBE                 1141
#define OBJ_TRANSYS_RIFFLE                 1140
#define OBJ_RENRAKU_BYPASS                 6109
#define OBJ_FOXFIRE_KITSUNE                1966
#define OBJ_TRANSYS_ARMOR                  1139
#define OBJ_MATRIX_SWORD                   387

#ifdef USE_PRIVATE_CE_WORLD
#define OBJ_CYB_CERAMIC_BONE_LACING        85066
#define OBJ_CYB_DATAJACK                   85012
#define OBJ_CYB_BOOSTED_REFLEXES_III_ALPHA 85270
#define OBJ_CYB_THERMOGRAPHIC_VISION       85053
#define OBJ_CYB_SMARTLINK_II_ALPHA         85318
#define OBJ_CYB_ENCEPHALON_II              85014
#define OBJ_CYB_MATH_SPU_III               85019
#define OBJ_CYB_SMARTLINK_II               85118
#else
#define OBJ_CYB_CERAMIC_BONE_LACING        463
#define OBJ_CYB_DATAJACK                   304
#define OBJ_CYB_BOOSTED_REFLEXES_III_ALPHA 327
#define OBJ_CYB_THERMOGRAPHIC_VISION       307
#define OBJ_CYB_SMARTLINK_II_ALPHA         548
#define OBJ_CYB_ENCEPHALON_II              1107
#define OBJ_CYB_MATH_SPU_III               1110
#define OBJ_CYB_SMARTLINK_II               302
#endif

#define OBJ_CYB_EYE_PACKAGE_LL_TH_FC_ALPHA 566

#ifdef USE_PRIVATE_CE_WORLD
#define OBJ_BIO_MUSCLE_TONER_III           85811
#define OBJ_BIO_MUSCLE_TONER_IV            85812
#define OBJ_BIO_CEREBRAL_BOOSTER_II        85927
#define OBJ_BIO_MUSCLE_AUGMENTATION_II     85806
#define OBJ_BIO_ENHANCED_ARTICULATION      85803
#define OBJ_BIO_TRAUMA_DAMPER              85940
#define OBJ_BIO_SYNAPTIC_ACCELERATOR_II    85939
#define OBJ_BIO_CEREBRAL_BOOSTER_II        85927
#else
#define OBJ_BIO_MUSCLE_TONER_III           60609
#define OBJ_BIO_MUSCLE_TONER_IV            60610
#define OBJ_BIO_CEREBRAL_BOOSTER_II        411
#define OBJ_BIO_MUSCLE_AUGMENTATION_II     417
#define OBJ_BIO_ENHANCED_ARTICULATION      415
#define OBJ_BIO_TRAUMA_DAMPER              60611
#define OBJ_BIO_SYNAPTIC_ACCELERATOR_II    414
#define OBJ_BIO_CEREBRAL_BOOSTER_II        411
#endif


#define OBJ_OLD_BLANK_MAGAZINE_FROM_CLASSIC 601

#define OBJ_SHOPCONTAINER                  83
#define OBJ_VEHCONTAINER                   13

#define BOTTOM_OF_TEMPLATE_ITEMS           106
#define OBJ_BLANK_OPTICAL_CHIP             106
#define OBJ_BLANK_PROGRAM_DESIGN           107
#define OBJ_BLANK_PROGRAM                  108
#define OBJ_BLANK_PHOTO                    109
#define OBJ_ROLL_OF_NUYEN                  110
#define OBJ_PIECE_OF_MAIL                  111
#define OBJ_BLANK_PART_DESIGN              112
#define OBJ_CUSTOM_CYBERDECK_SHELL         113
#define OBJ_SHAMANIC_LODGE                 114
#define OBJ_HERMETIC_CIRCLE                115
#define OBJ_CONJURING_MATERIALS            116
#define OBJ_BLANK_SPELL_FORMULA            117
#define OBJ_POCKET_SECRETARY_FOLDER        118
#define OBJ_NEOPHYTE_SUBSIDY_CARD          119
#define OBJ_DYNAMIC_GRAFFITI               120
#define OBJ_BLANK_AMMOBOX                  121
#define OBJ_CUSTOM_NERPS_CYBERWARE         122
#define OBJ_CUSTOM_NERPS_BIOWARE           123
#define OBJ_RITUAL_SPELL_COMPONENTS        124
#define TOP_OF_TEMPLATE_ITEMS              124

#define OBJ_BLANK_MAGAZINE                 127

#define OBJ_DOCWAGON_PAPER_GOWN            16201
#define OBJ_ANTI_DRUG_CHEMS                44

#define QST_MAGE_INTRO                     5743

#define MOB_BRAIN_IN_A_JAR                 10050

#define VEHICLE_JACKRABBIT_E               1302
#define VEHICLE_EUROCAR_WESTWIND_2000      1303
#define VEHICLE_FORD_AMERICAR              1305
#define VEHICLE_HARLEY_SCORPION            1307
#define VEHICLE_YAMAHA_RAPIER              1308
#define VEHICLE_FORD_BISON                 1309
#define VEHICLE_GMC_4201                   1321
#define VEHICLE_ARES_ROADMASTER            1322
#define VEHICLE_GMC_BULLDOG                1323
#define VEHICLE_TRANSPORT_PICKUP           1327
#define VEHICLE_RUNABOUT                   1320
#define VEHICLE_NOMAD_SUV                  12300
#define VEHICLE_BRUMBY_SUV                 12301
#define VEHICLE_GOPHER_PICKUP              12302
#define VEHICLE_WHITE_EAGLE_BIKE           12303
#define VEHICLE_DOBERMAN                   37500
#define VEHICLE_SNOOPER                    37501
#define VEHICLE_SURVEILLANCE               37502
#define VEHICLE_ROTODRONE                  37503
#define VEHICLE_DALMATION                  37504
#define VEHICLE_SUPERCOMBI_RV              39205


// Misc defines from spec_procs.cpp
#define LIBRARY_SKILL    3
#define NEWBIE_SKILL    6
#define NORMAL_MAX_SKILL  8
#define LEARNED_LEVEL    12
#define RENT_FACTOR 1

// Definitions for message history.
/* Adding a new channel? Do the following:
   - Add a COMM_CHANNEL_X definition for it and increment NUM_COMM_CH by 1.
   - Add a handler for it in raw_message_history() in act.comm.cpp.
   - Add a string for it in message_history_channels[] in constants.cpp.
 */

#define NUM_MESSAGES_TO_RETAIN     50

#define COMM_CHANNEL_HIRED         0
#define COMM_CHANNEL_NEWBIE        1
#define COMM_CHANNEL_OOC           2
#define COMM_CHANNEL_OSAYS         3
#define COMM_CHANNEL_PAGES         4
#define COMM_CHANNEL_PHONE         5
#define COMM_CHANNEL_RPE           6
#define COMM_CHANNEL_RADIO         7
#define COMM_CHANNEL_SAYS          8
#define COMM_CHANNEL_SHOUTS        9
#define COMM_CHANNEL_TELLS         10
#define COMM_CHANNEL_WTELLS        11
#define COMM_CHANNEL_EMOTES        12

#define NUM_COMMUNICATION_CHANNELS 13


/* Exit / error codes. */
#define EXIT_CODE_ZERO_ALL_IS_WELL                    0
#define ERROR_BITFIELD_SIZE_EXCEEDED                  10
#define ERROR_LIBSODIUM_INIT_FAILED                   11
#define ERROR_UNKNOWN_SUBCOMMAND_TO_INDEX_BOOT        12
#define ERROR_OPENING_INDEX_FILE                      13
#define ERROR_BOOT_ZERO_RECORDS_COUNTED               14
#define ERROR_ZONEREAD_PREMATURE_EOF                  15
#define ERROR_ZONEREAD_FORMAT_ERROR                   16
#define ERROR_MYSQL_DATABASE_NOT_FOUND                17
#define ERROR_ARRAY_OUT_OF_BOUNDS                     18
#define ERROR_CANNOT_RESOLVE_VNUM                     19
#define ERROR_DB_TABLE_REQUIRED                       20
#define ERROR_DB_COLUMN_REQUIRED                      21
#define ERROR_PROTOCOL_BUFFER_EXCEEDS_INPUT_LENGTH    22
#define ERROR_FAILED_TO_VALIDATE_ARGON2_TEST_PASSWORD 23
#define ERROR_HASHING_TOOK_TOO_LONG                   24
#define ERROR_FAILED_TO_MATCH_CRYPT_PASSWORD          25
#define ERROR_FAILED_TO_VALIDATE_CRYPT_PASSWORD       26
#define ERROR_FAILED_TO_CONVERT_CRYPT_TO_ARGON2       27
#define ERROR_FAILED_TO_MATCH_ARGON2                  28
#define ERROR_FAILED_TO_VALIDATE_ARGON2_PASSWORD      29
#define ERROR_OUT_OF_MEMORY_WHILE_HASHING             30
#define ERROR_CODER_DIDNT_SPECIFY_FIELD_TYPE          31
#define ERROR_INVALID_ARGUMENTS_TO_BINARY             32
#define ERROR_INVALID_DATA_DIRECTORY                  33
#define ERROR_COULD_NOT_CREATE_SOCKET                 34
#define ERROR_COULD_NOT_SET_SOCKET_OPTIONS            35
#define ERROR_COULD_NOT_REUSE_ADDRESS                 36
#define ERROR_COULD_NOT_REUSE_PORT                    37
#define ERROR_COULD_NOT_SET_LINGER                    38
#define ERROR_COULD_NOT_BIND_SOCKET                   39
#define ERROR_GETRLIMIT_FAILED                        40
#define ERROR_SYSCONF_FAILED                          41
#define ERROR_YOU_SET_AN_IMPOSSIBLE_PLAYER_LIMIT      42
#define ERROR_COULD_NOT_RECOVER_FROM_SELECT_SLEEP     43
#define ERROR_FNCTL_ENCOUNTERED_ERROR                 44
#define ERROR_UNABLE_TO_FREE_MEMORY_IN_CLEARSTACKS    45
#define ERROR_MALLOC_FAILED_IN_CREATE_MACRO           46
#define ERROR_UNABLE_TO_FIND_PUSH_COMMAND             47
#define ERROR_UNABLE_TO_CREATE_STR_IN_MAIL            48
#define ERROR_DATABASE_CHANGES_NEEDED                 49
#define ERROR_NULL_VEHICLE_VEH_FROM_ROOM              50
#define ERROR_MISSING_ELEMENTALS                      51
#define EXIT_CODE_REBOOTING                           52   /* what's so great about HHGTTG, anyhow? */
#define ERROR_WORLD_BOOT_FORMAT_ERROR                 53
#define ERROR_FLAG_CONSTANT_MISSING                   54

// Materials.
#define MATERIAL_PAPER        0
#define MATERIAL_WOOD         1
#define MATERIAL_GLASS        2
#define MATERIAL_FABRIC       3
#define MATERIAL_LEATHER      4
#define MATERIAL_BRICK        5
#define MATERIAL_PLASTIC      6
#define MATERIAL_ADV_PLASTICS 7
#define MATERIAL_METAL        8
#define MATERIAL_ORICHALCUM   9
#define MATERIAL_ELECTRONICS  10
#define MATERIAL_COMPUTERS    11
#define MATERIAL_TOXIC_WASTES 12
#define MATERIAL_ORGANIC      13
#define MATERIAL_STONE        14
#define MATERIAL_CERAMIC      15
#define MATERIAL_CONCRETE     16
#define NUM_MATERIALS         17

#define MAX_KEYRING_WEIGHT    1.0

// Note: If you change this, you'll have to update zone file loading etc as well.
#define NUM_ZONE_EDITOR_IDS   5

#define LIST_COMMAND_LIMIT 500

// Stack / memory canaries.
#define CANARY_VALUE 31337

// MySQL constraints.
#define MYSQL_SIGNED_TINYINT_MAX     127
#define MYSQL_UNSIGNED_TINYINT_MAX   255
#define MYSQL_SIGNED_SMALLINT_MAX    32767
#define MYSQL_UNSIGNED_SMALLINT_MAX  65535
#define MYSQL_SIGNED_MEDIUMINT_MAX   8388607
#define MYSQL_UNSIGNED_MEDIUMINT_MAX 16777215
#define MYSQL_SIGNED_INT_MAX         2147483647
#define MYSQL_UNSIGNED_INT_MAX       4294967295
// Bigint takes 2^63-1 signed, 2^64-1 unsigned. You probably don't need to care about it.

// Dominator constants.
#define DOMINATOR_MODE_DEACTIVATED 0
#define DOMINATOR_MODE_PARALYZER   1
#define DOMINATOR_MODE_ELIMINATOR  2
#define DOMINATOR_MODE_DECOMPOSER  3

#define MAX_RADIO_FREQUENCY 24
#define MIN_RADIO_FREQUENCY 6

#define FOOD_DRINK_MAX 24

#define CURRENT_BACKGROUND_COUNT   0
#define CURRENT_BACKGROUND_TYPE    1
#define PERMANENT_BACKGROUND_COUNT 2
#define PERMANENT_BACKGROUND_TYPE  3

#define VEH_DAM_THRESHOLD_LIGHT     1
#define VEH_DAM_THRESHOLD_MODERATE  3
#define VEH_DAM_THRESHOLD_SEVERE    6
#define VEH_DAM_THRESHOLD_DESTROYED 10
#define VEH_DAMAGE_NEEDS_WORKSHOP   7

// Don't change these.
#define STRING_ROOM_TITLE_UNFINISHED "An unfinished room"
#define STRING_ROOM_DESC_UNFINISHED "You are in an unfinished room.\r\n"
#define STRING_ROOM_JACKPOINT_NO_ADDR "An undisclosed location"

#define STRING_OBJ_NAME_UNFINISHED "unfinished object"
#define STRING_OBJ_RDESC_UNFINISHED "An unfinished object is lying here."
#define STRING_OBJ_SDESC_UNFINISHED "an unfinished object"
#define STRING_OBJ_LDESC_UNFINISHED "It looks pretty much like an unfinished object"

#define STRING_MOB_NAME_UNFINISHED "an unfinished mob"
#define STRING_MOB_KEYWORDS_UNFINISHED "mob unfinished"
#define STRING_MOB_RDESC_UNFINISHED "An unfinished mob stands here."
#define STRING_MOB_LDESC_UNFINISHED "It looks barely coherent as it waits to be created.\r\n"

#define HOST_COLOR_BLUE   0
#define HOST_COLOR_GREEN  1
#define HOST_COLOR_ORANGE 2
#define HOST_COLOR_RED    3
#define HOST_COLOR_BLACK  4

#define MOUNT_FIRMPOINT_INTERNAL 0
#define MOUNT_FIRMPOINT_EXTERNAL 1
#define MOUNT_HARDPOINT_INTERNAL 2
#define MOUNT_HARDPOINT_EXTERNAL 3
#define MOUNT_TURRET             4
#define MOUNT_MINITURRET         5

#define PC_CORPSE_BARRIER 75

#define ARMOR_PENALTY_TOTAL  4
#define ARMOR_PENALTY_HEAVY  3
#define ARMOR_PENALTY_MEDIUM 2
#define ARMOR_PENALTY_LIGHT  1
#define ARMOR_PENALTY_NONE   0

#define CLIMBING_TYPE_JUST_CLIMBING 0
#define CLIMBING_TYPE_WATER_WINGS   1

#define HOLSTER_TYPE_SMALL_GUNS     0
#define HOLSTER_TYPE_MELEE_WEAPONS  1
#define HOLSTER_TYPE_LARGE_GUNS     2

// For nuyen gain metrics.
#define NUYEN_INCOME_DECKING                0
#define NUYEN_INCOME_SHOP_SALES             1
#define NUYEN_INCOME_AUTORUNS               2
#define NUYEN_INCOME_LOOTED_FROM_NPCS       3
#define NUYEN_OUTFLOW_SHOP_PURCHASES        4
#define NUYEN_OUTFLOW_GENERIC_SPEC_PROC     5
#define NUYEN_OUTFLOW_VEHICLE_PURCHASES     6
#define NUYEN_OUTFLOW_CYBERWARE_INSTALL     7
#define NUYEN_OUTFLOW_INITIATION            8
#define NUYEN_OUTFLOW_HOUSING               9
#define NUYEN_OUTFLOW_METAMAGIC_TRAINING    10
#define NUYEN_OUTFLOW_STAT_TRAINING         11
#define NUYEN_OUTFLOW_SKILL_TRAINING        12
#define NUYEN_OUTFLOW_SPELL_TRAINING        13
#define NUYEN_OUTFLOW_AMMO_BUILDING         14
#define NUYEN_OUTFLOW_REPAIRS               15
#define NUYEN_OUTFLOW_WHERESMYCAR           16
#define NUYEN_INCOME_JUNKING                17
#define NUYEN_INCOME_TRADE_COMMAND          18
#define NUYEN_INCOME_IDLE_REWARD            19
#define NUYEN_OUTFLOW_DOCWAGON              20
#define NUYEN_OUTFLOW_LODGE_AND_CIRCLE      21
#define NUYEN_OUTFLOW_TAXIS                 22
#define NUYEN_OUTFLOW_PGROUP                23
#define NUYEN_OUTFLOW_TRADE_COMMAND         24
#define NUYEN_OUTFLOW_CREDSTICK_CRACKER     25
#define NUYEN_OUTFLOW_DEATH_PENALTY         26
#define NUYEN_OUTFLOW_DRUGS                 27
#define NUYEN_OUTFLOW_RITUAL_CASTING        28
#define NUYEN_INCOME_STAFF_PAYOUT           29
#define NUYEN_OUTFLOW_STAFF_CHARGE          30

#define NUM_OF_TRACKED_NUYEN_INCOME_SOURCES 31
// Make sure you update constants.cpp's nuyen_faucets_and_sinks[] too!

#define NI_IS_SINK   0
#define NI_IS_FAUCET 1

#define DIRTY_BIT_SKILLS      0
#define DIRTY_BIT_POWERS      1
#define DIRTY_BIT_SPELLS      2
#define DIRTY_BIT_METAMAGIC   3
#define DIRTY_BIT_ELEMENTALS  4
#define DIRTY_BIT_MEMORY      5
#define DIRTY_BIT_DRUG        6
#define DIRTY_BIT_ALIAS       7
#define NUM_DIRTY_BITS        8


#define SMARTLINK_II_MODIFIER 3
#define SMARTLINK_I_MODIFIER  2

#define DECK_FLAG_HAS_PART_POINTING_TO_IT (1 << 0)

// Don't actually change this value unless you're rewriting regeneration etc.
#define TEMP_QUI_LOSS_DIVISOR                 4

// Names for room->light[X] (also shadow, poltergeist, silence)
#define ROOM_NUM_SPELLS_OF_TYPE               0
#define ROOM_HIGHEST_SPELL_FORCE              1
#define ROOM_LIGHT_HEADLIGHTS_AND_FLASHLIGHTS 2

// Mode definitions for act.informative.cpp.
#define SHOW_MODE_ON_GROUND                0
#define SHOW_MODE_IN_INVENTORY             1
#define SHOW_MODE_INSIDE_CONTAINER         2
// 3 and 4 are not used, idk what they used to be.
#define SHOW_MODE_JUST_DESCRIPTION         5
#define SHOW_MODE_JUST_SHOW_GLOW_ETC       6
#define SHOW_MODE_OWN_EQUIPMENT            7
#define SHOW_MODE_CONTAINED_OBJ            8
#define SHOW_MODE_SOMEONE_ELSES_EQUIPMENT  9

#define BOTCHED_ROLL_RESULT                -1

// Set this at the end of every flag list in constants.cpp.
#define MAX_FLAG_MARKER "\n"

#define MAX_VISIBILITY_PENALTY             8

// This is to flag variables used eg in macros so that the compiler won't complain about unused variables
// when they're actually used. Casting to void does nothing but the compiler will stop thinking it's
// unused. This is the same way Q_UNUSED is used in Qt. -- Nodens
#define UNUSED(expr) (void)(expr)

#endif // _awake_h_
