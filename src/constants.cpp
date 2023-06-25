#include "structs.hpp"
#include "awake.hpp"
#include "newmagic.hpp"

const char *awakemud_version[] =
    {
      "AwakeMUD Community Edition, version 0.9.0 BETA\r\n"
    };


/* (Note: strings for class definitions in class.c instead of here) */
const char *status_ratings[] =
  {
    "Nonexistent",
    "Mortal",
    "Builder",
    "Architect",
    "Fixer",
    "Conspirator",
    "Executive",
    "Developer",
    "Vice-President",
    "Admin",
    "President",
    "\n"
  };

const char *patch_names[] =
  {
    "antidote",
    "stimulant",
    "tranq",
    "trauma"
  };

const char *dist_name[] =
  {
    "close by",
    "not far off",
    "in the distance",
    "almost out of sight",
    "staff-5",
    "staff-6",
    "staff-7",
    "staff-8",
    "staff-9",
    "staff-10"
  };

const char *wound_arr[] =
  {
    "None",
    "L",
    "M",
    "S",
    "D",
    "BUGD1",
    "BUGD2",
    "BUGD3",
    "BUGD4",
    "BUGD5"
  };

const char *wound_name[] =
  {
    "None",
    "Light",
    "Moderate",
    "Serious",
    "Deadly",
    "\n",
    "BUGDeadly2",
    "BUGDeadly3",
    "BUGDeadly4",
    "BUGDeadly5"
  };

int damage_array[] =
  {
    0,
    1,
    3,
    6,
    10,
  };

// names of materials objects may be made out of
const char *material_names[] =
  {
    "paper",
    "wood",
    "glass",
    "fabric",
    "leather",
    "brick",            // 5
    "plastic",
    "adv. plastics",
    "metal",
    "orichalcum",
    "electronics",      // 10
    "computers",
    "toxic wastes",
    "organic",
    "stone",
    "ceramic",          // 15
    "concrete",
    "\n"
  };

const char *barrier_names[] =
  {
    "Standard glass",
    "Cheap material",
    "Average material/ballistic glass",
    "Heavy material",
    "Reinforced/armored glass",
    "Structural material",
    "Heavy structural material",
    "Armored/reinforced material",
    "Hardened material",
    "\n"
  };

int barrier_ratings[] =
  {
    2,
    3,
    4,
    6,
    8,
    12,
    16,
    24,
    32
  };

/* cardinal directions */
const char *dirs[] =
  {
    "north",
    "ne",
    "east",
    "se",
    "south",
    "sw",
    "west",
    "nw",
    "up",
    "down",
    "\n"
  };

const char *exitdirs[] =
  {
    "n",
    "ne",
    "e",
    "se",
    "s",
    "sw",
    "w",
    "nw",
    "u",
    "d",
    "\n"
  };

const char *lookdirs[] =
  {
    "north",
    "east",
    "south",
    "west",
    "ne",
    "se",
    "sw",
    "nw",
    "up",
    "down",
    "\n"
  };

  const char *fulllookdirs[] =
    {
      "north",
      "east",
      "south",
      "west",
      "northeast",
      "southeast",
      "southwest",
      "northwest",
      "up",
      "down",
      "\n"
    };

int convert_look[] =
  {
    0,
    2,
    4,
    6,
    1,
    3,
    5,
    7,
    8,
    9,
    10,
  };

int convert_dir[] =
  {
    1,
    7,
    2,
    8,
    3,
    9,
    4,
    10,
    5,
    6,
    0
  };

const char *fulldirs[] =
  {
    "north",
    "northeast",
    "east",
    "southeast",
    "south",
    "southwest",
    "west",
    "northwest",
    "up",
    "down",
    "\n"
  };

const char *thedirs[] =
  {
    "the north",
    "the northeast",
    "the east",
    "the southeast",
    "the south",
    "the southwest",
    "the west",
    "the northwest",
    "above",
    "below",
    "\n"
  };


/* ROOM_x */
const char *room_bits[] =
  {
    "DARK",
    "DEATH",
    "!MOB",
    "INDOORS",
    "PEACEFUL",
    "SOUNDPROOF",
    "!TRACK",
    "!MAGIC",
    "TUNNEL",
    "ARENA",
    "STREETLIGHTS",
    "HOUSE",
    "HCRSH",
    "ATRIUM",
    "OLC",
    "*",                          /* BFS MARK */
    "LOW_LIGHT",
    "!USED", // Empty slot.
    "!RADIO",
    "!BIKE",
    "!WALK",
    "FALL",
    "ROAD",
    "GARAGE",
    "STAFF-ONLY",
    "!QUIT",
    "SENT",
    "ASTRAL",
    "!GRID",
    "STORAGE",
    "!TRAFFIC",
    "ELEVATOR_SHAFT",
    "SOCIALIZE!",
    "CORPSE_SAVE_HACK",
    "STERILE",
    "SMALL_DRONE_ONLY",
    "RADIOACTIVE",
    "ALL_VEH_ACCESS",
    MAX_FLAG_MARKER
  };


/* EX_x */
const char *exit_bits[] =
  {
    "DOOR",
    "CLOSED",
    "LOCKED",
    "PICKPROOF",
    "DESTROYED",
    "HIDDEN",
    "WARDED",
    "TEMPORARY",
    "WINDOW",
    "BARRED_WINDOW",
    "CANT_SHOOT_THROUGH",
    MAX_FLAG_MARKER
  };


/* SEX_x */
const char *genders[] =
  {
    "Neutral",
    "Male",
    "Female",
    MAX_FLAG_MARKER
  };

const char *genders_decap[] =
  {
    "neutral",
    "male",
    "female",
    MAX_FLAG_MARKER
  };

const char *thrdgenders[] =
  {
    "it",
    "him",
    "her",
    MAX_FLAG_MARKER
  };

struct spirit_table spirits[] =
  {
    {"Hearth spirit", 30
    },
    {"City spirit", 29},
    {"Field spirit", 31},
    {"Forest spirit", 33},
    {"Desert spirit", 32},
    {"Mountain spirit", 34},
    {"River spirit", 39},
    {"Sea spirit", 40},
    {"Prairie spirit", 35},
    {"Mist spirit", 36},
    {"Storm spirit", 37},
    {"Wind spirit", 42},
    {"Lake spirit", 38},
    {"Swamp spirit", 41}
  };

const char *spirit_name[] =
  {
    "Inside",
    "City",
    "Field",
    "Forest",
    "Desert",
    "Mountain",
    "River",
    "Sea",
    "Prairie",
    "Mist",
    "Storm",
    "Wind",
    "Lake",
    "Swamp",
    "\n"
  };

const char *spirit_powers[] =
  {
    "accident",
    "alienate",
    "aura",
    "bind",
    "breathe",
    "conceal",
    "confuse",
    "engulf",
    "fear",
    "find",
    "guard",
    "manifest",
    "project"
  };

const char *spirit_powers_from_bit[] =
  {
    "Confusion",
    "Concealment",
    "Engulfment",
    "Movement Increase",
    "Movement Decrease"
  };

/* POS_x */
const char *position_types[] =
  {
    "Dead",
    "Mortally wounded",
    "Stunned",
    "Sleeping",
    "Lying",
    "Resting",
    "Sitting",
    "Fighting",
    "Standing",
    "\n"
  };

const char *attack_types[] =
  {
    "Hit",
    "Sting",
    "Whip",
    "Slash",
    "Bite",
    "Bludgeon",
    "Crush",
    "Pound",
    "Claw",
    "Maul",
    "Thrash",
    "Pierce",
    "Punch",
    "Stab",
    "Taser",
    "Shuriken",
    "Throwing knife",
    "Arrow/bolt",
    "Hand Grenade",
    "Grenade Launcher",
    "Rocket",
    "Pistol",
    "Blast",
    "Rifle",
    "Shotgun",
    "Machine Gun",
    "Cannon",
    "Bifurcate",
    "\n"
  };

const char *weapon_types[] =
  {
    "slashing weapon",
    "club",
    "piercing weapon",
    "whip",
    "glove",
    "hold-out pistol",
    "light pistol",
    "machine pistol",
    "heavy pistol",
    "taser",
    "submachine gun",
    "sport rifle",
    "sniper rifle",
    "assault rifle",
    "shotgun",
    "light machine gun",
    "medium machine gun",
    "heavy machine gun",
    "assault cannon",
    "minigun",
    "grenade launcher",
    "missile launcher",
    "revolver",
    "grenade",
    "\n"
  };

/* PLR_x */
const char *player_bits[] =
  {
    "KILLER",
    "BLACKLIST",
    "FROZEN",
    "DONTSET",
    "NEWBIE",
    "JUST_DIED",
    "VISA",
    "CEYE_ESS_DONE",
    "SITEOK",
    "!SHOUT",
    "!TITLE",
    "DELETED",
    "!DELETE",
    "PACKING",
    "!STAT",
    "IN-CHARGEN",
    "INVST",
    "!USED",
    "!USED",
    "OLC",
    "MATRIX",
    "PERCEIVE",
    "PROJECT",
    "SWITCHED",
    "WRITING",
    "MAILING",
    "EDITING",
    "SPELL-CREATE",
    "CUSTOMIZE",
    "!SNOOP",
    "WANTED",
    "NOOOC",
    "!AUTH",
    "EDITCON",
    "REMOTE",
    "INIT",
    "DRIVEBY",
    "RPE",
    "NOIDLEOUT",
    "TELLS_MUTED",
    "NEWBIE_MUTED",
    "IS_CYBERDOC",
    "PAID_FOR_CLOSECOMBAT",
    "PAID_FOR_KIPUP",
    "PAID_FOR_ROLLS",
    "NO_AUTO_SYSP",
    "RADIO_MUTED",
    "SITE_HIDDEN",
    "ENABLED_DRUGS",
    "SENT_DOCWAGON",
    "PAID_FOR_VNUMS",
    "DOCWAGON_READY",
    "\n"
  };

/* MOB_x */
const char *action_bits[] =
  {
    "SPEC",
    "SENTINEL",
    "SCAVENGER",
    "ISNPC",
    "AWARE",
    "AGGR",
    "STAY-ZONE",
    "WIMPY",
    "AGGR_ORK",
    "AGGR_ELF",
    "AGGR_DWARF",
    "MEMORY",
    "HELPER",
    "!RAM",
    "DUAL",
    "!EXPLODE",
    "AGGR_TROLL",
    "!BLIND",
    "ASTRAL",
    "GUARD",
    "AGGR_HUMAN",
    "SNIPER",
    "PRIVATE",
    "TRACK",
    "FLAMEAURA",
    "SPIRITGUARD",
    "SPIRITSTUDY",
    "SPIRITSORCERY",
    "AZTECHNOLOGY",
    "RENRAKU",
    "!KILL",
    "TOTALINVIS",
    "INANIMATE",
    "EMPLACED",
    "RACE_AGGR_VS_MOBS",
    "BROKE_ASS",
    "PERCEIVING",
    MAX_FLAG_MARKER
  };

/* PRF_x v2 */

struct preference_bit_struct preference_bits_v2[] = {
  // TODO: Add an 'inverted' flag for things like !OOC.
  // TODO: Is there a better way to name these? Or sort by channel / log / etc?
  // NAME                    STAFF  True: ON/OFF, False: YES/NO
  { "Pacify"               , TRUE , TRUE  },
  { "Compact"              , FALSE, TRUE  },
  { "Auto Exits"           , FALSE, TRUE  },
  { "Fight Gag"            , FALSE, TRUE  },
  { "Move Gag"             , FALSE, TRUE  },
  { "Deaf"                 , FALSE, TRUE  },
  { "No Tells"             , FALSE, TRUE  },
  { "No Radio"             , FALSE, TRUE  },
  { "No Newbie"            , FALSE, TRUE  },
  { "No Repeat"            , FALSE, TRUE  },
  { "Hardcore"             , FALSE, TRUE  },
  { "PKer"                 , FALSE, FALSE },
  { "On Quest"             , FALSE, FALSE },
  { "AFK"                  , FALSE, TRUE  },
  { "Suppress Staff Radio" , TRUE , TRUE  },
  { "UNUSED-15"            , FALSE, TRUE  },
  { "No Hassle"            , TRUE , TRUE  },
  { "Roomflags"            , TRUE , TRUE  },
  { "Holylight"            , TRUE , TRUE  },
  { "Connlog"              , TRUE , TRUE  },
  { "Deathlog"             , TRUE , TRUE  },
  { "Misclog"              , TRUE , TRUE  },
  { "Wizlog"               , TRUE , TRUE  },
  { "Syslog"               , TRUE , TRUE  },
  { "Zonelog"              , TRUE , TRUE  },
  { "Long Exits"           , FALSE, TRUE  },
  { "Rolls"                , FALSE, TRUE  },
  { "No OOC"               , FALSE, TRUE  },
  { "Auto Invis"           , TRUE , TRUE  },
  { "Cheatlog"             , TRUE , TRUE  },
  { "Assist"               , FALSE, TRUE  },
  { "Banlog"               , TRUE , TRUE  },
  { "No RPE"               , FALSE, TRUE  },
  { "No Hired"             , FALSE, TRUE  },
  { "Gridlog"              , TRUE , TRUE  },
  { "Wrecklog"             , TRUE , TRUE  },
  { "Questor"              , FALSE, TRUE  },
  { "NewbieHelper"         , FALSE, TRUE  },
  { "Menu Gag"             , FALSE, TRUE  },
  { "Longweapon"           , FALSE, FALSE },
  { "PGrouplog"            , TRUE , TRUE  },
  { "ShowGroupTag"         , FALSE, FALSE },
  { "Keepalive"            , FALSE, TRUE  },
  { "Screenreader"         , FALSE, FALSE },
  { "No Color"             , FALSE, TRUE  },
  { "No Prompt"            , FALSE, TRUE  },
  { "Helplog"              , TRUE , TRUE  },
  { "Purgelog"             , TRUE , TRUE  },
  { "No Autokill"          , FALSE, TRUE  },
  { "Voice Names"          , TRUE , TRUE  },
  { "FuckupLog"            , TRUE , TRUE  },
  { "EconLog"              , TRUE , TRUE  },
  { "Brief"                , TRUE , TRUE  },
  { "Highlights"           , TRUE , TRUE  },
  { "Pseudolanguage"       , TRUE , TRUE  },
  { "No Idle Nuyen Line"   , FALSE, TRUE  },
  { "Cyberdocs Allowed"    , FALSE, TRUE  },
  { "No Void on Idle"      , FALSE, TRUE  },
  { "RadLog"               , TRUE , TRUE  },
  { "Anonymous on Where"   , FALSE, TRUE  },
  { "IgnoreLog"            , TRUE , TRUE  },
  { "Sees Newbie Tips"     , FALSE, TRUE  },
  { "AutoStand"            , FALSE, TRUE  },
  { "AutoKipUp"            , FALSE, TRUE  },
  { "No Weather"           , FALSE, TRUE  },
  { "No XTERM-256 Color"   , FALSE, TRUE  },
  { "Client-Configurable Color", FALSE, TRUE  },
  { "Don't Alert Doctors on Mort", FALSE, TRUE  },
  { "MailLog"              , TRUE , TRUE  },
  { "\n"                   , 0    , 0     }
};

/* PRF_x */
const char *preference_bits[] =
  {
    "PACIFY",
    "COMPACT",
    "AUTOEX",
    "FIGHTGAG",
    "MOVEGAG",
    "DEAF",
    "!TELL",
    "!RADIO",
    "!NEWBIE",
    "!REPEAT",
    "HARDCORE",
    "PKER",
    "QUEST",
    "AFK",
    "STAFF_RADSUPPRESS",
    "UNUSED-15",
    "NOHASSLE",
    "ROOMFLAGS",
    "HOLYLIGHT",
    "CONNLOG",
    "DEATHLOG",
    "MISCLOG",
    "WIZLOG",
    "SYSLOG",
    "ZONELOG",
    "LONGEXIT",
    "ROLLS",
    "!OOC",
    "AUTOINVIS",
    "CHEATLOG",
    "ASSIST",
    "BANLOG",
    "!RPE",
    "!HIRED",
    "GRIDLOG",
    "WRECKLOG",
    "QUESTOR",
    "NEWBIEHELPER",
    "MENUGAG",
    "LONGWEAPON",
    "PGROUPLOG",
    "SHOWGROUPTAG",
    "KEEPALIVE",
    "SCREENREADER",
    "!COLOR",
    "!PROMPT",
    "HELPLOG",
    "PURGELOG",
    "!AUTOKILL",
    "NO_RADIO_NAMES",
    "FUCKUPLOG",
    "ECONLOG",
    "BRIEF",
    "!HIGHLIGHTS",
    "!PSEUDOLANGUAGE",
    "!IDLE_NUYEN_REWARD_MESSAGE",
    "CYBERDOC_PERMITTED",
    "NO_VOID",
    "RADLOG",
    "ANONYMOUS_ON_WHERE",
    "IGNORELOG",
    "TIPS",
    "AUTOSTAND",
    "AUTOKIPUP",
    "!WEATHER",
    "!XTERM256",
    "COERCE_ANSI",
    "ALERT_DOCTORS_ON_MORT",
    "MAILLOG",
    MAX_FLAG_MARKER
  };

/* AFF_x */
const char *affected_bits[] =
  {
    "NOTHING",
    "Ruthenium",
    "Banishing",
    "Ultrasound",
    "Prone",
    "Manifest",
    "Healed",
    "Group",
    "Damaged",
    "Infravision",
    "Fear",
    "ResistPain",
    "Detox",
    "LL-eyes",
    "Laser-sight",
    "Sneak",
    "Hide",
    "Vision x1",
    "Charm",
    "ACTION",
    "Vision x2",
    "Vision x3",
    "COUNTERATTACK",
    "Stabilize",
    "Currently Ritual Casting",
    "Thermoptic",
    "Acid",
    "APPROACH",
    "Pilot",
    "Rigger",
    "Manning",
    "Designing",
    "Programming",
    "Part Designing",
    "Part Building",
    "Withdrawal (Forced)",
    "Withdrawal",
    "Bonding Focus",
    "Conjuring",
    "Building Lodge",
    "Drawing Circle",
    "Packing Workshop",
    "Invis (Spell)",
    "ImpInvis (Spell)",
    "Tracking",
    "Tracked",
    "Binding",
    "Spell Design",
    "Surprised",
    "Ammo Building",
    "Engaging in Close Combat",
    "Tries for Close Combat",
    "Levitate",
    "Flame Aura",
    "Voice Modulator",
    "Wearing DocWagon Receiver",
    "Cheatlog Mark"
  };

/* CON_x */
const char *connected_types[] =
  {
    "Playing",                                    // 0
    "Disconnecting",
    "Get name",
    "Confirm name",
    "Get password",
    "Get new PW",                                 // 5
    "Confirm new PW",
    "CharGen",
    "Reading MOTD",
    "Main Menu",
    "Creating Part",                              // 10
    "Changing PW 1",
    "Changing PW 2",
    "Changing PW 3",
    "Self-Delete 1",
    "Self-Delete 2",                              // 15
    "Quit menu",
    "Changing PW Q1",
    "Changing PW Q2",
    "Changing PW Q3",
    "Self-Delete Q1",                             // 20
    "Self-Delete Q2",
    "Creating Spell",
    "Persona Editing",
    "Astral Editing",
    "Physical Editing",      // 25
    "Vehicle Editing",
    "Item Editing",
    "Room Editing",
    "Mob Editing",
    "Quest Editing",      // 30
    "Shop Editing",
    "Zone Editing",
    "Host Editing",
    "IC Editing",
    "Creating Program",
    "Creating Deck",      // 35
    "Creating Spell",
    "Initiation Menu",
    "Decorating",
    "Using PDA",
    "Vehicle Customizing",
    "Background Customizing",
    "Trideo Message Adding",
    "Creating Ammo",
    "Asking name",
    "Playergroup editing",
    "Helpfile editing",
    "\n"
  };

/* WEAR_x - for eq list */
const char *where[] =
  {
    "<used as light>      ",
    "<worn on head>       ",
    "<eyes>               ",
    "<ear>                ",
    "<ear>                ",
    "<worn on face>       ",
    "<worn around neck>   ",
    "<worn around neck>   ",
    "<worn over shoulder> ",
    "<worn about body>    ",
    "<worn on body>       ",
    "<worn underneath>    ",
    "<worn on arms>       ",
    "<slung under arm>    ",
    "<slung under arm>    ",
    "<worn around wrist>  ",
    "<worn around wrist>  ",
    "<worn on hands>      ",
    "<wielded>            ",
    "<held>               ",
    "<worn as shield>     ",
    "<worn on finger>     ",
    "<worn on finger>     ",
    "<worn on finger>     ",
    "<worn on finger>     ",
    "<worn on finger>     ",
    "<worn on finger>     ",
    "<worn on finger>     ",
    "<worn on finger>     ",
    "<belly button>       ",
    "<worn about waist>   ",
    "<worn on thigh>      ",
    "<worn on thigh>      ",
    "<worn on legs>       ",
    "<worn around ankle>  ",
    "<worn around ankle>  ",
    "<worn as socks>      ",
    "<worn on feet>       ",
    "<patch>              ",
    "<worn as underwear>  ",
    "<worn on the chest>  ",
    "<lapel>              ",
    "\n"
  };


const char *hands[] =
  {
    "<hand>               ",
    "<off hand>           ",
    "<both hands>         ",
  };

const char *wielding_hands[] =
  {
    "<wielded>            ",
    "<wielded (offhand)>  ",
    "<both hands>         ",
  };

const char *short_where[] =
  {
    "LIGHT",
    "HEAD",
    "EYES",
    "EAR1",
    "EAR2",
    "FACE",
    "NECK1",
    "NECK2",
    "BACK",
    "ABOUT",
    "BODY",
    "UNDER",
    "ARMS",
    "ARM1",
    "ARM2",
    "R_WRIST",
    "L_WRIST",
    "HANDS",
    "WIELD",
    "HELD",
    "SHIELD",
    "FINGER1",
    "FINGER2",
    "FINGER3",
    "FINGER4",
    "FINGER5",
    "FINGER6",
    "FINGER7",
    "FINGER8",
    "BELLY",
    "WAIST",
    "THIGH1",
    "THIGH2",
    "LEGS",
    "ANKLE1",
    "ANKLE2",
    "SOCK",
    "FEET",
    "PATCH",
    "UNDERWEAR",
    "CHEST/UNDER",
    "LAPEL"
  };

/* ITEM_x (ordinal object types) */
const char *item_types[] =
  {
    "UNDEFINED",
    "Light",
    "Workshop",
    "Camera",
    "Deck Part",
    "Weapon",
    "Bow/X-bow",
    "Arrow/bolt",
    "Custom Deck",
    "Gyro Harness",
    "Drug",
    "Worn",
    "Other",
    "Magic Tool",
    "DocWagon",
    "Container",
    "Radio",
    "Liq Container",
    "Key",
    "Food",
    "Money",
    "Phone",
    "Bioware",
    "Fountain",
    "Cyberware",
    "Cyberdeck",
    "Program",
    "Gun Magazine",
    "Gun Accessory",
    "Spell Formula",
    "Kit",
    "Focus",
    "Patch",
    "Climbing gear",
    "Quiver",
    "Decking Accessory",
    "Rigger Control Deck",
    "Skillsoft",
    "Vehicle Mod",
    "Holster",
    "Program Design",
    "Quest Item",
    "Ammo Box",
    "Keyring",
    "Shop Container",
    "Vehicle Container",
    "Graffiti",
    "Destroyable",
    "Loaded Decoration",
    "\n"
  };

/* ITEM_WEAR_ (wear bitvector) */
const char *wear_bits[] =
  {
    "TAKE",
    "FINGER",
    "NECK",
    "BODY",
    "HEAD",
    "LEGS",
    "FEET",
    "HANDS",
    "ARMS",
    "SHIELD",
    "ABOUT",
    "WAIST",
    "WRIST",
    "WIELD",
    "HOLD",
    "EYES",
    "EAR",
    "UNDER",
    "BACK",
    "ANKLE",
    "SOCK",
    "BELLY",
    "UNDERARM",
    "FACE",
    "THIGH",
    "PATCH",
    "UNDERWEAR",
    "CHEST/UNDER",
    "LAPEL",
    "\n"
  };

/* ITEM_x (extra bits) */
const char *extra_bits[] =
  {
    "GLOW",
    "HUM",
    "!RENT",
    "!DONATE",
    "!INVIS",
    "INVISIBLE",
    "MAGIC",
    "!DROP",
    "FORMFIT",
    "!SELL",
    "CORPSE",
    "GOD_ONLY",
    "TWO_HANDS",
    "COMPLEXBURST",
    "VOLATILE",
    "WIZLOAD",
    "NOTROLL",
    "NOELF",
    "NODWARF",
    "NOORK",
    "NOHUMAN",
    "SNIPER",
    "IMMLOAD",
    "NERPS",
    "BLOCKS_ARMOR",
    "HARDENED_ARMOR",
    "DONT_TOUCH",
    "MAGIC_INCOMPATIBLE",
    "KEPT",
    "AIR_FILTRATION",
    "NBC_RESIST",
    "NBC_IMMUNE",
    "PURGE_ON_DEATH",
    "CHEATLOG_MARK",
    MAX_FLAG_MARKER
  };

const char *pc_readable_extra_bits[] =
  {
    "Glowing",
    "Humming",
    "Doesn't Save",
    "Can't be Donated",
    "Can't be Made Invis",
    "Invisible",
    "Looks Magical when Perceiving",
    "Can't be Dropped",
    "Form-Fitting",
    "Can't be Sold",
    "It's a Corpse",
    "Staff-Only Item",
    "Requires Two Hands",
    "Complex Burst (NERP)",
    "Volatile (NERP)",
    "Loaded by Staff (!)",
    "Can't be Used by Trolls or Troll Metatypes",
    "Can't be Used by Elves or Elf Metatypes",
    "Can't be Used by Dwarves or Dwarf Metatypes",
    "Can't be Used by Orks or Ork Metatypes",
    "Can't be Used by Humans",
    "Sniper Rifle",
    "Loaded by Staff (!!)",
    "No Coded Effect (NERP)",
    "Prevents Wearing Other Armor",
    "Hardened Armor",
    "Derived from Template Item",
    "Incompatible with Magic",
    "Kept",
    "Filters Air (NERP)",
    "Resists Bio/Chemical Weapons (NERP)",
    "Immune to Bio/Chemical Weapons (NERP)",
    "Vanishes on Death",
    "Doesn't Save", // Deliberate -- concealing cheatlog mark
    "\n"
  };

/* APPLY_x */
const char *apply_types[] =
  {
    "Nothing",
    "Body",
    "Quickness",
    "Strength",
    "Charisma",
    "Intelligence",
    "Willpower",
    "Essence",
    "Magic",
    "Reaction",
    "Age",
    "Weight",
    "Height",
    "Max Mental",
    "Max Physical",
    "Nothing",
    "Ballistic",
    "Impact",
    "Astral Pool",
    "Nothing",
    "Combat Pool",
    "Hacking Pool",
    "Magic Pool",
    "Init Dice",
    "Target Numbers",
    "Control Pool",
    "Task Pool (Bod)",
    "Task Pool (Qui)",
    "Task Pool (Str)",
    "Task Pool (Cha)",
    "Task Pool (Int)",
    "Task Pool (Wil)",
    "Task Pool (Rea)"
  };

struct program_data programs[] =
  {
    {"None", 0
    },
    {"Bod", 3 },
    {"Evasion", 3 },
    {"Masking", 2 },
    {"Sensor", 2 },
    {"Attack", 1 },
    {"Slow", 4 },
    {"Medic", 4 },
    {"Snooper", 2 },
    {"BattleTac", 5 },
    {"Compressor", 2 },
    {"Analyze", 3 },
    {"Decrypt", 1 },
    {"Deception", 2 },
    {"Relocate", 2 },
    {"Sleaze", 3 },
    {"Scanner", 3 },
    {"Browse", 1 },
    {"Read/Write", 2 },
    {"Track", 8 },
    {"Armor", 3 },
    {"Camo", 3 },
    {"Crash", 3 },
    {"Defuse", 2 },
    {"Evaluate", 2 },
    {"Validate", 4 },
    {"Swerve", 3 },
    {"Programming Suite", 15 },
    {"Commlink", 1 },
    {"Cloak", 3 },
    {"Lock-On", 3  },
    {"Cold ASIST Interface", 2 },
    {"Hot ASIST Interface", 4 },
    {"Hardening", 8 },
    {"ICCM Filter", 4 },
    {"Icon Chip", 2 },
    {"MPCP", 8 },
    {"Reality Filter", 10 },
    {"Response Increase", 1 }
  };

int attack_multiplier[] = { 0, 2, 3, 4, 5 };

struct part_data parts[] =
  {
    { "Nothing", 0, 0, 0
    },
    { "Active Memory", TYPE_KIT, 0, -1 },
    { "Storage Memory", TYPE_KIT, 0, -1 },
    { "Hot ASIST Interface", TYPE_KIT, SOFT_ASIST_HOT, 2 },
    { "Cold ASIST Interface", TYPE_KIT, SOFT_ASIST_COLD, 2 },
    { "Hardening", TYPE_WORKSHOP, SOFT_HARDENING, 3 },
    { "ICCM Biofeedback Filter", TYPE_WORKSHOP, SOFT_ICCM, 1 },
    { "Icon Chip", TYPE_KIT, SOFT_ICON, 0 },
    { "I/O Speed", TYPE_KIT, 0, 0 },
    { "MPCP", TYPE_WORKSHOP, SOFT_MPCP, 0 },
    { "Bod Chip", TYPE_KIT, SOFT_BOD, 0 },
    { "Sensor Chip", TYPE_KIT, SOFT_SENSOR, 0 },
    { "Masking Chip", TYPE_KIT, SOFT_MASKING, 1 },
    { "Evasion Chip", TYPE_KIT, SOFT_EVASION, 1 },
    { "Ports (FUPS)", TYPE_KIT, 0, -1 },
    { "RAS Override", TYPE_KIT, 0, 0 },
    { "Reality Filters", TYPE_WORKSHOP, SOFT_REALITY, 2 },
    { "Response Increase", TYPE_WORKSHOP, SOFT_RESPONSE, 1 },
    { "Matrix Interface", TYPE_KIT, 0, -1 },
    { "Maser Interface", TYPE_KIT, 0, 0 },
    { "Cellular Interface", TYPE_KIT, 0, 1 },
    { "Laser Interface", TYPE_KIT, 0, 0 },
    { "Microwave Interface", TYPE_WORKSHOP, 0, 1},
    { "Radio Interface", TYPE_KIT, 0, 0 },
    { "Satellite Interface", TYPE_WORKSHOP, 0, 2 },
    { "Signal Amplifier", TYPE_KIT, 0, -1 },

  };
const char *log_types[] =
  {
    "CONNLOG",
    "DEATHLOG",
    "MISCLOG",
    "WIZLOG",
    "SYSLOG",
    "ZONELOG",
    "CHEATLOG",
    "WIZITEMLOG",
    "BANLOG",
    "GRIDLOG",
    "WRECKLOG",
    "PGROUPLOG",
    "HELPLOG",
    "PURGELOG",
    "FUCKUPLOG",
    "ECONLOG",
    "RADLOG",
    "IGNORELOG",
    "MAILLOG",
    "\n"
  };

/* CONT_x */
const char *container_bits[] =
  {
    "CLOSEABLE",
    "PICKPROOF",
    "CLOSED",
    "LOCKED",
    "\n",
  };

/* one-word alias for each drink */
/* LIQ_x */
/* New and improved liquids by Jordan */
const char *drinks[] =
  {
    "water",
    "soda",
    "coffee",
    "milk",
    "juice",
    "tea",
    "soykaf",
    "smoothie",
    "sports juice",
    "espresso",
    "beer",
    "ale",
    "lager",
    "import beer",
    "microbrew beer",
    "malt liquor",
    "cocktail",
    "margarita",
    "liquor",
    "grain alcohol",
    "vodka",
    "tequila",
    "brandy",
    "rum",
    "whiskey",
    "gin",
    "champagne",
    "red wine",
    "white wine",
    "blood",
    "hot sauce",
    "piss",
    "local speciality",
    "fuckup juice",
    "cleaning solution",
    "hot cocoa",
    "coolant",
    "motor oil",
    "battery acid",
    "\n"
  };

/* one-word alias for each drink */
const char *drinknames[] =
  {
    "water",
    "soda",
    "coffee",
    "milk",
    "juice",
    "tea",
    "soykaf",
    "smoothie",
    "sportsjuice",
    "espresso",
    "beer",
    "ale",
    "lager",
    "import",
    "microbrew",
    "maltliquor",
    "cocktail",
    "margarita",
    "liquor",
    "everclear",
    "vodka",
    "tequila",
    "brandy",
    "rum",
    "whiskey",
    "gin",
    "champagne",
    "redwine",
    "whitewine",
    "blood",
    "hotsauce",
    "piss",
    "local",
    "fuckup",
    "cleaner",
    "cocoa",
    "coolant",
    "oil",
    "pain",
    "\n"
  };


/* effect of drinks on hunger, thirst, and drunkenness -- see values.doc */
int drink_aff[][3] = {
                       {0, 1, 10},     // Water
                       {0, 1, 5},      // Soda
                       {0, 1, 4},      // Coffee
                       {0, 3, 6},      // Milk
                       {0, 1, 5},      // Juice
                       {0, 1, 6},      // Tea
                       {0, 2, 6},      // Soykaf
                       {0, 2, 3},      // Smoothie
                       {0, 1, 11},     // Sports Juice
                       {0, 1, 2},      // Espresso
                       {3, 2, -1},     // Beer
                       {3, 2, -1},     // Ale
                       {3, 2, -1},     // Lager
                       {3, 2, 0},      // Import
                       {3, 2, -1},     // Microbrew
                       {5, 3, -2},     // Malt Liquor
                       {6, 0, 1},      // Cocktail
                       {6, 0, 1},      // Margarita
                       {8, 0, 1},      // Liquor
                       {12,0, -1},     // Everclear
                       {8, 0, 1},      // Vodka
                       {9, 0, 0},      // Tequila
                       {7, 0, -1},     // Brandy
                       {8, 0, 0},      // Rum
                       {8, 0, -1},     // Whiskey
                       {7, 0, 1},      // Gin
                       {5, 0, 1},      // Champagne
                       {4, 1, 2},      // Red Wine
                       {4, 1, 2},      // White Wine
                       {0, 2, -1},     // Blood
                       {0, 1, -1},     // Hot Sauce
                       {0, 1, -4},     // Urine
                       {6, 0, -1},     // Local
                       {12, 0, -1},    // Fuckup Juice
                       {24, -24, -24}, // Cleaning Fluid
                       {0, 1, 5},      // Hot Cocoa
                       {10, 1, 1},     // Coolant
                       {0, -10, 10},   // Motor Oil
                       {24, -24, -24}  // Battery Acid
                     };


/* color of the various drinks */
const char *color_liquid[] =
  {
    "clear",    // Water
    "dark",    // Soda
    "dark brown",   // Coffee
    "white",    // Milk
    "colourful",   // Juice
    "golden",    // Tea
    "white",    // Soykaf
    "colourful",   // Smoothie
    "clear",    // Sports Juice
    "black",    // Espresso
    "brown",    // Beer
    "dark brown",   // Ale
    "brown",    // Lager
    "light brown",  // Import
    "brown",    // Microbrew
    "brownish",     // Malt Liquor
    "colourful",   // Cocktail
    "colourful",   // Margarita
    "clear",    // Liquor
    "clear",    // Everclear
    "clear",    // Vodka
    "golden brown", // Tequila
    "amber",    // Brandy
    "dark brown",   // Rum
    "dark amber",   // Whiskey
    "clear",    // Gin
    "yellowish",   // Champagne
    "dark red",   // Red Wine
    "light yellow", // White Wine
    "dark red",   // Blood
    "blazing red",  // Hot Sauce
    "bright yellow", // Urine
    "dark amber",    // Local
    "thick",         // fuckup juice
    "clear",         // cleaning solution
    "rich brown",    // Cocoa
    "vivid blue",    // coolant
    "thick yellow",  // motor oil
    "acrid yellow",  // battery acid
  };

const char *fullness[] =
  {
    "less than half ",
    "about half ",
    "more than half ",
    ""
  };

const char *adept_powers[] =
  {
    "None",  // 0
    "Astral Perception",
    "Combat Sense",
    "Blind Fighting",
    "Quick Strike",
    "Killing Hands",  // 5
    "Nerve Strike",
    "Smashing Blow",
    "Distance Strike",
    "Increased Reflexes",
    "Boosted Strength",  // 10
    "Boosted Quickness",
    "Boosted Body",
    "Improved Strength",
    "Improved Quickness",
    "Improved Body",    // 15
    "Improved Perception",
    "Improved Sense (Low Light)",
    "Improved Sense (Thermographic)",
    "Improved Sense (Magnification)",
    "Magic Resistance",  // 20
    "Pain Resistance",
    "Temperature Tolerance",
    "Spell Shroud",
    "True Sight",
    "Missile Parry",  // 25
    "Missile Mastery",
    "Mystic Armor",
    "Rapid Healing",
    "Freefall",
    "Iron Will",  // 30
    "Aid Spell",
    "Empathic Healing",
    "Kinesics",
    "Linguistics",
    "Living Focus",  // 35
    "Motion Sense",
    "Pain Relief",
    "Penetrating Strike",
    "Side Step",
    "Sustenance",  // 40
    "Counterstrike",
    "Animal Empathy", // NERP Adept Powers from here on - Vile
    "Body Control",
    "Commanding Voice",
    "Cool Resolve",  // 45
    "Deep Rooting",
    "Delay Damage (Obvious)",
    "Delay Damage (Silent)",
    "Eidetic Sense Memory",
    "Elemental Strike",  // 50
    "Empathic Reading",
    "Empathic Sense",
    "Enhanced Balance",
    "Enthralling Performance",
    "Facial Sculpt",  // 55
    "Flexibility",
    "Gliding",
    "Great Leap",
    "Improved Sense (Flare Compensation)",
    "Improved Sense (High Freq Hearing)", // 60
    "Improved Sense (Low Freq Hearing)",
    "Improved Sense (Sound Dampening)",
    "Improved Sense (Sound Filtering)",
    "Improved Sense (Spatial Recognition)",
    "Improved Sense (Enhanced Hearing)",  // 65
    "Improved Sense (Directional Sense)",
    "Improved Sense (Improved Scent)",
    "Improved Sense (Taste)",
    "Inertia Strike",
    "Iron Gut",  // 70
    "Iron Lungs",
    "Magic Sense",
    "Melanin Control",
    "Multi Tasking",
    "Nimble Fingers",  // 75
    "Quick Draw",
    "Resilience",
    "Rooting",
    "Sixth Sense",
    "Sprint",  // 80
    "Suspended State",
    "Three Dimensional Memory",
    "Traceless Walk",
    "Voice Control",
    "Wall Running",  // 85
    "\n"
  };

struct skill_data skills[] =
  {
    // name,                       linked attribute, active/knowledge,     magic, group, reflex, nodflt
    {"OMGWTFBBQ",                               BOD, SKILL_TYPE_ACTIVE,    FALSE,    99,  FALSE,  FALSE },
    {"Athletics",                               BOD, SKILL_TYPE_ACTIVE,    FALSE,    99,  FALSE,  FALSE },
    {"Armed Combat",                            STR, SKILL_TYPE_ACTIVE,    FALSE,    99,  FALSE,  FALSE },
    {"Edged Weapons",                           STR, SKILL_TYPE_ACTIVE,    FALSE,     2,  TRUE ,  FALSE },
    {"Pole Arms",                               STR, SKILL_TYPE_ACTIVE,    FALSE,     2,  TRUE ,  FALSE },
    {"Whips and Flails",                        QUI, SKILL_TYPE_ACTIVE,    FALSE,    99,  TRUE ,  FALSE },
    {"Clubs",                                   STR, SKILL_TYPE_ACTIVE,    FALSE,     2,  TRUE ,  FALSE },
    {"Brawling",                                STR, SKILL_TYPE_ACTIVE,    FALSE,     4,  TRUE ,  FALSE },
    {"Centering",                               WIL, SKILL_TYPE_ACTIVE,    TRUE ,    99,  FALSE,  FALSE },
    {"Cyber Implants",                          STR, SKILL_TYPE_ACTIVE,    FALSE,     4,  TRUE ,  FALSE },
    {"Firearms",                                STR, SKILL_TYPE_ACTIVE,    FALSE,    99,  FALSE,  FALSE },
    {"Pistols",                                 QUI, SKILL_TYPE_ACTIVE,    FALSE,     5,  TRUE ,  FALSE },
    {"Rifles",                                  QUI, SKILL_TYPE_ACTIVE,    FALSE,     5,  TRUE ,  FALSE },
    {"Shotguns",                                QUI, SKILL_TYPE_ACTIVE,    FALSE,     5,  TRUE ,  FALSE },
    {"Assault Rifles",                          QUI, SKILL_TYPE_ACTIVE,    FALSE,     5,  TRUE ,  FALSE },
    {"Submachine Guns",                         QUI, SKILL_TYPE_ACTIVE,    FALSE,     5,  TRUE ,  FALSE },
    {"Grenade Launchers",                       INT, SKILL_TYPE_ACTIVE,    FALSE,     6,  FALSE,  FALSE },
    {"Tasers",                                  QUI, SKILL_TYPE_ACTIVE,    FALSE,    99,  TRUE ,  FALSE },
    {"Mounted Gunnery",                         INT, SKILL_TYPE_ACTIVE,    FALSE,     6,  FALSE,  FALSE },
    {"Machine Guns",                            STR, SKILL_TYPE_ACTIVE,    FALSE,     1,  TRUE ,  FALSE },
    {"Missile Launchers",                       INT, SKILL_TYPE_ACTIVE,    FALSE,     6,  FALSE,  FALSE },
    {"Assault Cannons",                         STR, SKILL_TYPE_ACTIVE,    FALSE,     1,  TRUE ,  FALSE },
    {"Artillery",                               STR, SKILL_TYPE_ACTIVE,    FALSE,    99,  FALSE,  FALSE },
    {"Projectile Weapons",                      STR, SKILL_TYPE_ACTIVE,    FALSE,    99,  TRUE ,  FALSE },
    {"Oral Strike",                             QUI, SKILL_TYPE_ACTIVE,    FALSE,    99,  FALSE,  FALSE },
    {"Spell Design",                            INT, SKILL_TYPE_KNOWLEDGE, TRUE ,    99,  FALSE,  FALSE },
    {"Throwing Weapons",                        STR, SKILL_TYPE_ACTIVE,    FALSE,    99,  TRUE ,  FALSE },
    {"Enchanting",                              WIL, SKILL_TYPE_ACTIVE,    TRUE ,    99,  FALSE,  FALSE },
    {"Cyberterminal Design",                    INT, SKILL_TYPE_KNOWLEDGE, FALSE,    99,  FALSE,  FALSE },
    {"Demolitions",                             INT, SKILL_TYPE_ACTIVE,    FALSE,    99,  FALSE,  FALSE },
    {"Computers",                               INT, SKILL_TYPE_ACTIVE,    FALSE,     7,  FALSE,  FALSE },
    {"Electronics",                             INT, SKILL_TYPE_ACTIVE,    FALSE,     7,  TRUE ,  FALSE },
    {"Computer Building and Repair",            INT, SKILL_TYPE_ACTIVE,    FALSE,    99,  FALSE,  FALSE },
    {"Biotech",                                 INT, SKILL_TYPE_ACTIVE,    FALSE,    99,  TRUE ,  FALSE },
    {"Electronics Building and Repair",         INT, SKILL_TYPE_ACTIVE,    FALSE,    99,  FALSE,  FALSE },
    {"Talismongering",                          INT, SKILL_TYPE_KNOWLEDGE, TRUE ,    99,  FALSE,  FALSE },
    {"Police Procedures",                       INT, SKILL_TYPE_KNOWLEDGE, FALSE,    99,  FALSE,  FALSE },
    {"Leadership",                              CHA, SKILL_TYPE_ACTIVE,    FALSE,    99,  FALSE,  FALSE },
    {"Interrogation",                           CHA, SKILL_TYPE_ACTIVE,    FALSE,     8,  FALSE,  FALSE },
    {"Negotiation",                             CHA, SKILL_TYPE_ACTIVE,    FALSE,    99,  FALSE,  FALSE },
    {"Chanting",                                CHA, SKILL_TYPE_ACTIVE,    TRUE ,    99,  FALSE,  FALSE },
    {"Conjuring",                               WIL, SKILL_TYPE_ACTIVE,    TRUE ,    99,  FALSE,  TRUE  },
    {"Sorcery",                                 WIL, SKILL_TYPE_ACTIVE,    TRUE ,    99,  FALSE,  TRUE  },
    {"Legerdemain",                             QUI, SKILL_TYPE_ACTIVE,    FALSE,    99,  FALSE,  FALSE },
    {"Corporate Etiquette",                     CHA, SKILL_TYPE_ACTIVE,    FALSE,    99,  FALSE,  FALSE },
    {"Media Etiquette",                         CHA, SKILL_TYPE_ACTIVE,    FALSE,    99,  FALSE,  FALSE },
    {"Street Etiquette",                        CHA, SKILL_TYPE_ACTIVE,    FALSE,    99,  FALSE,  FALSE },
    {"Tribal Etiquette",                        CHA, SKILL_TYPE_ACTIVE,    FALSE,    99,  FALSE,  FALSE },
    {"Elf Etiquette",                           CHA, SKILL_TYPE_ACTIVE,    FALSE,    99,  FALSE,  FALSE },
    {"Combat Program Design",                   INT, SKILL_TYPE_KNOWLEDGE, FALSE,    99,  FALSE,  FALSE },
    {"Defensive Program Design",                INT, SKILL_TYPE_KNOWLEDGE, FALSE,    99,  FALSE,  FALSE },
    {"Operational Program Design",              INT, SKILL_TYPE_KNOWLEDGE, FALSE,    99,  FALSE,  FALSE },
    {"Special Program Design",                  INT, SKILL_TYPE_KNOWLEDGE, FALSE,    99,  FALSE,  FALSE },
    {"Cyberterminal Program Design",            INT, SKILL_TYPE_KNOWLEDGE, FALSE,    99,  FALSE,  FALSE },
    {"Data Brokerage",                          INT, SKILL_TYPE_KNOWLEDGE, FALSE,    99,  FALSE,  FALSE },
    {"Aura Reading",                            INT, SKILL_TYPE_ACTIVE,    TRUE ,    99,  FALSE,  TRUE  },
    {"Stealth",                                 QUI, SKILL_TYPE_ACTIVE,    FALSE,    99,  TRUE ,  FALSE },
    {"Steal",                                   QUI, SKILL_TYPE_ACTIVE,    FALSE,    99,  FALSE,  FALSE },
    {"Track",                                   INT, SKILL_TYPE_ACTIVE,    FALSE,    99,  FALSE,  FALSE },
    {"Climbing",                                BOD, SKILL_TYPE_ACTIVE,    FALSE,    99,  FALSE,  FALSE },
    {"Driving Motorcycles",                     REA, SKILL_TYPE_ACTIVE,    FALSE,    99,  TRUE ,  FALSE },
    {"zzzzzzzzzzzzzz",                          REA, SKILL_TYPE_ACTIVE,    FALSE,    99,  FALSE,  FALSE }, // unused skill
    {"Driving Cars",                            REA, SKILL_TYPE_ACTIVE,    FALSE,    10,  TRUE ,  FALSE },
    {"Driving Trucks",                          REA, SKILL_TYPE_ACTIVE,    FALSE,    99,  TRUE ,  FALSE },
    {"Repairing Motorcycles",                   INT, SKILL_TYPE_ACTIVE,    FALSE,    99,  FALSE,  FALSE },
    {"Repairing Cars",                          INT, SKILL_TYPE_ACTIVE,    FALSE,    99,  FALSE,  FALSE },
    {"Repairing Drones",                        INT, SKILL_TYPE_ACTIVE,    FALSE,    99,  FALSE,  FALSE },
    {"Repairing Trucks",                        INT, SKILL_TYPE_ACTIVE,    FALSE,    99,  FALSE,  FALSE },
    {"Dancing",                                 QUI, SKILL_TYPE_ACTIVE,    FALSE,    99,  FALSE,  FALSE },
    {"Singing",                                 INT, SKILL_TYPE_ACTIVE,    FALSE,    99,  FALSE,  FALSE },
    {"Instrument",                              INT, SKILL_TYPE_ACTIVE,    FALSE,    99,  FALSE,  FALSE },
    {"Arcane Language",                         INT, SKILL_TYPE_KNOWLEDGE, TRUE ,    99,  FALSE,  FALSE },
    {"Meditation",                              WIL, SKILL_TYPE_ACTIVE,    TRUE ,    99,  FALSE,  FALSE },
    {"English",                                 INT, SKILL_TYPE_KNOWLEDGE, FALSE,    99,  FALSE,  FALSE },
    {"Sperethiel",                              INT, SKILL_TYPE_KNOWLEDGE, FALSE,    99,  FALSE,  FALSE },
    {"Spanish",                                 INT, SKILL_TYPE_KNOWLEDGE, FALSE,    99,  FALSE,  FALSE },
    {"Japanese",                                INT, SKILL_TYPE_KNOWLEDGE, FALSE,    99,  FALSE,  FALSE },
    {"Chinese",                                 INT, SKILL_TYPE_KNOWLEDGE, FALSE,    99,  FALSE,  FALSE },
    {"Korean",                                  INT, SKILL_TYPE_KNOWLEDGE, FALSE,    99,  FALSE,  FALSE },
    {"Italian",                                 INT, SKILL_TYPE_KNOWLEDGE, FALSE,    99,  FALSE,  FALSE },
    {"Russian",                                 INT, SKILL_TYPE_KNOWLEDGE, FALSE,    99,  FALSE,  FALSE },
    {"Sioux",                                   INT, SKILL_TYPE_KNOWLEDGE, FALSE,    99,  FALSE,  FALSE },
    {"Makaw",                                   INT, SKILL_TYPE_KNOWLEDGE, FALSE,    99,  FALSE,  FALSE },
    {"Crow",                                    INT, SKILL_TYPE_KNOWLEDGE, FALSE,    99,  FALSE,  FALSE },
    {"Salish",                                  INT, SKILL_TYPE_KNOWLEDGE, FALSE,    99,  FALSE,  FALSE },
    {"Ute",                                     INT, SKILL_TYPE_KNOWLEDGE, FALSE,    99,  FALSE,  FALSE },
    {"Navajo",                                  INT, SKILL_TYPE_KNOWLEDGE, FALSE,    99,  FALSE,  FALSE },
    {"German",                                  INT, SKILL_TYPE_KNOWLEDGE, FALSE,    99,  FALSE,  FALSE },
    {"Or'zet",                                  INT, SKILL_TYPE_KNOWLEDGE, FALSE,    99,  FALSE,  FALSE },
    {"Arabic",                                  INT, SKILL_TYPE_KNOWLEDGE, FALSE,    99,  FALSE,  FALSE },
    {"Latin",                                   INT, SKILL_TYPE_KNOWLEDGE, FALSE,    99,  FALSE,  FALSE },
    {"Gaelic",                                  INT, SKILL_TYPE_KNOWLEDGE, FALSE,    99,  FALSE,  FALSE },
    {"French",                                  INT, SKILL_TYPE_KNOWLEDGE, FALSE,    99,  FALSE,  FALSE },
    {"Animal Handling",                         CHA, SKILL_TYPE_ACTIVE,    FALSE,    99,  FALSE,  FALSE },
    {"Animal Taming",                           CHA, SKILL_TYPE_ACTIVE,    FALSE,    99,  FALSE,  FALSE },
    {"Edged Weapon Repair",                     INT, SKILL_TYPE_ACTIVE,    FALSE,    99,  FALSE,  FALSE },
    {"Polearm Repair",                          INT, SKILL_TYPE_ACTIVE,    FALSE,    99,  FALSE,  FALSE },
    {"Club Repair",                             INT, SKILL_TYPE_ACTIVE,    FALSE,    99,  FALSE,  FALSE },
    {"Throwing Weapon Repair",                  INT, SKILL_TYPE_ACTIVE,    FALSE,    99,  FALSE,  FALSE },
    {"Whips/Flail Repair",                      INT, SKILL_TYPE_ACTIVE,    FALSE,    99,  FALSE,  FALSE },
    {"Projectile Repair",                       INT, SKILL_TYPE_ACTIVE,    FALSE,    99,  FALSE,  FALSE },
    {"Pistol Repair and Ammo Crafting",         INT, SKILL_TYPE_ACTIVE,    FALSE,    13,  FALSE,  FALSE },
    {"Shotgun Repair and Ammo Crafting",        INT, SKILL_TYPE_ACTIVE,    FALSE,    13,  FALSE,  FALSE },
    {"Rifle Repair and Ammo Crafting",          INT, SKILL_TYPE_ACTIVE,    FALSE,    13,  FALSE,  FALSE },
    {"Heavy Weapon Repair and Ammo Crafting",   INT, SKILL_TYPE_ACTIVE,    FALSE,    99,  FALSE,  FALSE },
    {"Submachine Gun Repair and Ammo Crafting", INT, SKILL_TYPE_ACTIVE,    FALSE,    13,  FALSE,  FALSE },
    {"Armor Repair",                            INT, SKILL_TYPE_ACTIVE,    FALSE,    99,  FALSE,  FALSE },
    {"Off-hand Edged Weapons",                  STR, SKILL_TYPE_ACTIVE,    FALSE,    99,  FALSE,  FALSE },
    {"Off-hand Clubs",                          STR, SKILL_TYPE_ACTIVE,    FALSE,    99,  FALSE,  FALSE },
    {"Off-hand Cyber Implants",                 STR, SKILL_TYPE_ACTIVE,    FALSE,    99,  FALSE,  FALSE },
    {"Off-hand Whips/Flails",                   QUI, SKILL_TYPE_ACTIVE,    FALSE,    99,  FALSE,  FALSE },
    {"Survival",                                WIL, SKILL_TYPE_ACTIVE,    FALSE,    99,  FALSE,  FALSE },
    {"Land Navigation",                         INT, SKILL_TYPE_KNOWLEDGE, FALSE,    99,  FALSE,  FALSE },
    {"Water Navigation",                        INT, SKILL_TYPE_KNOWLEDGE, FALSE,    99,  FALSE,  FALSE },
    {"Air Navigation",                          INT, SKILL_TYPE_KNOWLEDGE, FALSE,    99,  FALSE,  FALSE },
    {"Small Unit Tactics",                      INT, SKILL_TYPE_ACTIVE,    FALSE,    99,  FALSE,  FALSE },
    {"Chemistry",                               INT, SKILL_TYPE_KNOWLEDGE, FALSE,    99,  FALSE,  FALSE },
    {"Diving",                                  BOD, SKILL_TYPE_ACTIVE,    FALSE,    99,  FALSE,  FALSE },
    {"Parachuting",                             BOD, SKILL_TYPE_ACTIVE,    FALSE,    99,  FALSE,  FALSE },
    {"Underwater Combat",                       STR, SKILL_TYPE_ACTIVE,    FALSE,    99,  FALSE,  FALSE },
    {"Piloting Rotorcraft",                     REA, SKILL_TYPE_ACTIVE,    FALSE,     9,  FALSE,  FALSE },
    {"Piloting Fixed Wing Aircraft",            REA, SKILL_TYPE_ACTIVE,    FALSE,     9,  FALSE,  FALSE },
    {"Piloting Vector Thrust Aircraft",         REA, SKILL_TYPE_ACTIVE,    FALSE,     9,  FALSE,  FALSE },
    {"Acting",                                  CHA, SKILL_TYPE_ACTIVE,    FALSE,    99,  FALSE,  FALSE },
    {"Disguise",                                INT, SKILL_TYPE_ACTIVE,    FALSE,    99,  FALSE,  FALSE },
    {"Lock Picking",                            QUI, SKILL_TYPE_ACTIVE,    FALSE,    99,  FALSE,  FALSE },
    {"Riding",                                  QUI, SKILL_TYPE_ACTIVE,    FALSE,    99,  FALSE,  FALSE },
    {"Spray Weapons",                           QUI, SKILL_TYPE_ACTIVE,    FALSE,    99,  FALSE,  FALSE },
    {"Intimidation",                            CHA, SKILL_TYPE_ACTIVE,    FALSE,     8,  FALSE,  FALSE },
    {"Gun Cane",                                QUI, SKILL_TYPE_ACTIVE,    FALSE,    99,  FALSE,  FALSE },
    {"Bracer Gun",                              QUI, SKILL_TYPE_ACTIVE,    FALSE,    99,  FALSE,  FALSE },
    {"Blowgun",                                 QUI, SKILL_TYPE_ACTIVE,    FALSE,    99,  FALSE,  FALSE },
    {"Pharmaceuticals",                         INT, SKILL_TYPE_ACTIVE,    FALSE,    99,  FALSE,  FALSE },
    {"Hebrew",                                  INT, SKILL_TYPE_KNOWLEDGE, FALSE,    99,  FALSE,  FALSE },
    {"Iroquois",                                INT, SKILL_TYPE_KNOWLEDGE, FALSE,    99,  FALSE,  FALSE },
    {"Medicine",                                INT, SKILL_TYPE_KNOWLEDGE, FALSE,    99,  FALSE,  FALSE },
    {"Repairing Fixed Wing Aircraft",           INT, SKILL_TYPE_ACTIVE,    FALSE,    99,  FALSE,  FALSE },
    {"Repairing Rotorcraft",                    INT, SKILL_TYPE_ACTIVE,    FALSE,    99,  FALSE,  FALSE },
    {"Repairing Vector Thrust Aircraft",        INT, SKILL_TYPE_ACTIVE,    FALSE,    99,  FALSE,  FALSE },
    {"Repairing Hovercraft",                    INT, SKILL_TYPE_ACTIVE,    FALSE,    99,  FALSE,  FALSE },
    {"Repairing Motorboats",                    INT, SKILL_TYPE_ACTIVE,    FALSE,    99,  FALSE,  FALSE },
    {"Repairing Ships",                         INT, SKILL_TYPE_ACTIVE,    FALSE,    99,  FALSE,  FALSE },
    {"Repairing Lighter-than-Air Aircraft",     INT, SKILL_TYPE_ACTIVE,    FALSE,    99,  FALSE,  FALSE },
    {"Piloting Hovercraft",                     REA, SKILL_TYPE_ACTIVE,    FALSE,    99,  FALSE,  FALSE },
    {"Piloting Motorboats",                     REA, SKILL_TYPE_ACTIVE,    FALSE,    12,  FALSE,  FALSE },
    {"Piloting Ships",                          REA, SKILL_TYPE_ACTIVE,    FALSE,    12,  FALSE,  FALSE },
    {"Piloting Lighter-than-Air Aircraft",      REA, SKILL_TYPE_ACTIVE,    FALSE,     9,  FALSE,  FALSE },
    {"Mechanical Arm Operation",                REA, SKILL_TYPE_ACTIVE,    FALSE,    11,  FALSE,  FALSE },
    {"Piloting Semiballistic Aircraft",         REA, SKILL_TYPE_ACTIVE,    FALSE,     9,  FALSE,  FALSE },
    {"Piloting Suborbital Craft",               REA, SKILL_TYPE_ACTIVE,    FALSE,     9,  FALSE,  FALSE },
    {"Piloting Tracked Vehicles",               REA, SKILL_TYPE_ACTIVE,    FALSE,    10,  FALSE,  FALSE },
    {"Piloting Walkers",                        REA, SKILL_TYPE_ACTIVE,    FALSE,    11,  FALSE,  FALSE }
  };

int rev_dir[] =
  {
    4,
    5,
    6,
    7,
    0,
    1,
    2,
    3,
    9,
    8
  };

const char *weekdays[7] =
  {
    "Mon",
    "Tue",
    "Wed",
    "Thu",
    "Fri",
    "Sat",
    "Sun"
};

const char *weekdays_tm_aligned[7] =
  {
    "Sun",
    "Mon",
    "Tue",
    "Wed",
    "Thu",
    "Fri",
    "Sat"
  };

const char *month_name[12] =
  {
    "January",
    "February",
    "March",
    "April",
    "May",
    "June",
    "July",
    "August",
    "September",
    "October",
    "November",
    "December"
  };

const char *veh_types[NUM_VEH_TYPES] =
  {
    "Drone",
    "Bike",
    "Car",
    "Truck",
    "Fixed Wing",
    "Rotorcraft",
    "Vector Thrust",
    "Hovercraft",
    "Motorboat",
    "Ship",
    "Lighter Than Air",
    "Semi-Ballistic",
    "Sub-Orbital",
    "Tracked Vehicle",
    "Walker"
  };

struct mod_data mod_types[NUM_MODTYPES] =
  {
    { "NOTHING", 0 },
    { "Engine", TYPE_FACILITY },
    { "Nitrous Oxide Injectors", TYPE_FACILITY },
    { "Turbocharger", TYPE_WORKSHOP },
    { "Autonav", TYPE_FACILITY },
    { "CMC", TYPE_FACILITY },
    { "Drive-By-Wire", TYPE_FACILITY },
    { "Improved Suspension", TYPE_WORKSHOP },
    { "Motorbike Gyroscope", TYPE_FACILITY },
    { "Offroad Suspension", TYPE_FACILITY },
    { "Armor", TYPE_FACILITY },
    { "Concealed Armor", TYPE_FACILITY },
    { "Crashcage", TYPE_WORKSHOP },
    { "Rollbars", TYPE_WORKSHOP },
    { "Thermal Masking", TYPE_FACILITY },
    { "Mounts", TYPE_FACILITY },
    { "ECM", TYPE_FACILITY },
    { "ECCM", TYPE_FACILITY },
    { "Sensors", TYPE_FACILITY },
    { "Seats", TYPE_WORKSHOP },
    { "Tires", TYPE_KIT },
    { "Other", TYPE_KIT },
    { "Ammo Bin", TYPE_KIT },
    { "Pokeystick", TYPE_WORKSHOP }

  };
const char *mod_name[NUM_MODS] =
  {
    "NOWHERE",
    "Intake (Front)",
    "Intake (Mid)",
    "Intake (Rear)",
    "Engine",
    "Exhaust",
    "Cooling",
    "Brakes",
    "Suspension",
    "Wheels",
    "Tires",
    "Body (Front)",
    "Body (Drivers Side)",
    "Body (Passengers Side)",
    "Body (Rear)",
    "Body (Hood)",
    "Body (Roof)",
    "Body (Spoiler)",
    "Body (Windscreen)",
    "Body (Windows)",
    "Armor",
    "Seats",
    "Computer1",
    "Computer2",
    "Computer3",
    "Phone",
    "Radio",
    "Mounts"
  };

const char *engine_types[NUM_ENGINE_TYPES] =
  {
     "Nothing",
     "Electric",
     "Fuel Cell",
     "Gasoline",
     "Methane",
     "Diesel"
  };

const char *veh_aff[] =
  {
    "Nothing",
    "Handling",
    "Speed",
    "Acceleration",
    "Body",
    "Armor",
    "Sig",
    "Autonav",
    "Seating (Front)",
    "Seating (Rear)",
    "Load",
    "Sensors",
    "Pilot",
    "Ultrasound"
  };

const char *veh_flag[] =
  {
    "Nothing",
    "Can Fly",
    "Amphibious",
    "Workshop",
    "Newbie",
    "Ultrasound"
  };

const char *jurisdictions[] =
  {
    "Seattle",
    "Portland",
    "Caribbean",
    "Ocean",
    "\n"
  };

const char *host_color[] =
  {
    "^BBlue",
    "^GGreen",
    "^YOrange",
    "^RRed",
    "^LBlack"
  };

const char *host_type[] =
  {
    "Datastore",
    "LTG",
    "RTG",
    "PLTG",
    "Security",
    "Controller"
  };

const char *intrusion[] =
  {
    "Easy",
    "Average",
    "Hard"
  };

int host_subsystem_acceptable_ratings[][2] =
  {
    {8, 10},  // Easy
    {10, 15}, // Average
    {13, 18}, // Hard
    {-1, -1}  // Error case
  };

const char *alerts[] =
  {
    "No Alert",
    "Passive",
    "Active",
    "Shutdown"
  };

const char *ic_type[] =
  {
    "^WCrippler^n",
    "^WKiller^n",
    "^WProbe^n",
    "^WScramble^n",
    "^WTar Baby^n",
    "^WScout^n",
    "Trace",
    "Blaster",
    "Ripper",
    "Sparky",
    "Tar Pit",
    "^LLethal Black^n",
    "^LNon-Lethal Black^n"
  };

const char *ic_option[] =
  {
    "UNDEF",
    "ARMOR",
    "CASCADING",
    "EXPERT DEFENSE",
    "EXPERT OFFENSE",
    "SHIELD",
    "SHIFT",
    "TRAP"
  };

const char *ic_option_long[] =
  {
    "Undefined",
    "Armor, ",
    "Cascading Attack, ",
    "Expert Defense, ",
    "Expert Offense, ",
    "Shield, ",
    "Shift, ",
    "Trap  "
  };

// Weight and cost are PER ROUND now.
struct ammo_data ammo_type[] =
  {
    // name          tn   time   weight  cost   s. index
    {"normal",        2,    1,    .025,    2,    0.75},
    {"APDS",         14,   14,    .025,    7,    4.0 },
    {"explosive",     3,  1.5,    .075,    5,    0.8 },
    {"EX",            6,    3,    .075,   10,    1.5 },
    {"flechette",     3,  1.5,    .05,    10,    0.8 },
    {"gel",           4,    2,    .025,    3,    1.0 },
    {"harmless",      1,  0.1,    .010,    1,    0.5 },
    {"anti-vehicle", 16,   14,    .100,   20,    4.0 }
  };

/* House rule: Ammo costs for various weapons vary based on assumed caliber. */
float weapon_type_ammo_cost_multipliers[] = {
  0, 0, 0, 0, 0, // melee weapons have a multiplier of 0 (edged, club, polearm, whip, glove)
  1.0,  // holdout pistol
  1.0,  // light pistol
  1.0,  // machine pistol
  1.0,  // heavy pistol
  1.0,  // taser
  1.0,  // smg
  1.0,  // sport rifle
  1.0,  // sniper rifle
  1.0,  // assault rifle
  1.0,  // shotgun
  1.0,  // lmg
  1.0,  // mmg
  1.0,  // hmg
  9.0,  // cannon (per SR3 p281, derived from 45 / explosive cost)
  1.0,  // minigun
  1.0,  // grenade launcher (placeholder value)
  1.0,  // missile launcher (placeholder value)
  1.0,  // revolver
  1.0   // grenade (placeholder value)
};

const char *positions[] =
  {
    "is lying here, dead",
    "is lying here, mortally wounded",
    "is lying here, stunned",
    "is sleeping here",
    "is lying here",
    "is resting here",
    "is sitting here",
    "!FIGHTING!",
    "is standing here"
  };

int racial_limits[][2][6] = {
                              {{ 0, 0, 0, 0, 0, 0 },{ 0, 0, 0, 0, 0, 0 }}, // undef
                              {{ 0, 0, 0, 0, 0, 0 },{ 0, 0, 0, 0, 0, 0 }}, // undef
                              {{ 6, 6, 6, 6, 6, 6 },{ 9, 9, 9, 9, 9, 9 }}, //human
                              {{ 7, 6, 8, 6, 6, 7 },{ 11, 9, 12, 9, 9, 11 }}, //dwarf
                              {{ 6, 7, 6, 8, 6, 6 },{ 9, 11, 9, 12, 9, 9 }}, // elf
                              {{ 9, 6, 8, 5, 5, 6 },{ 14, 9 , 12, 8, 8, 9 }}, // ork
                              {{ 11, 5, 10, 4, 4, 6 },{ 17, 8, 15, 6, 6, 9 }}, // troll
                              {{ 11, 5, 12, 4, 4, 6 },{ 17, 8, 18, 6, 6, 9 }}, // cyclops
                              {{ 7, 6, 8, 6, 6, 7 },{ 11, 9, 12, 9, 9, 11 }}, // koborokuru
                              {{ 10, 5, 9, 6, 4, 6 },{ 15, 8, 13, 8, 6, 9 }}, // fomori
                              {{ 8, 6, 7, 6, 6, 7 },{ 12, 9, 11, 9, 9, 11 }}, // menehune
                              {{ 8, 6, 8, 5, 6, 6 },{ 12, 9, 12, 8, 9, 9 }}, // hobgoblin
                              {{ 11, 5, 11, 4, 4, 6 },{ 17, 8, 17, 6, 6, 9 }}, // giant
                              {{ 7, 6, 7, 6, 6, 8 },{ 11, 9, 11, 9, 9, 12 }}, // gnome
                              {{ 8, 6, 8, 5, 5, 7 },{ 12, 9, 12, 8, 8, 11 }}, // oni
                              {{ 6, 6, 6, 8, 6, 7 },{ 9, 9, 9, 12, 9, 11 }}, // wakyambi
                              {{ 9, 6, 8, 6, 5, 6 },{ 14, 9, 12, 9, 8, 9 }}, // ogre
                              {{ 10, 5, 9, 5, 5, 6 },{ 15, 8, 14, 8, 8, 9 }},  // minotaur
                              {{ 9, 5, 8, 5, 5, 7 },{ 14, 8, 12, 8, 8, 11 }}, // satyr
                              {{ 6, 8, 6, 8, 6, 6 },{ 9, 12, 9, 12, 9, 9 }} // night one
                            };

int racial_attribute_modifiers[][6] = {
//  BOD QUI STR CHA INT WIL
  {  0,  0,  0,  0,  0,  0  }, // undef
  {  0,  0,  0,  0,  0,  0  }, // undef
  {  0,  0,  0,  0,  0,  0  }, // human
  {  1,  0,  2,  0,  0,  1  }, // dwarf
  {  0,  1,  0,  2,  0,  0  }, // elf
  {  3,  0,  2, -1, -1,  0  }, // ork
  {  5, -1,  4, -2, -2,  0  }, // troll
  {  5, -1,  6, -2, -2,  0  }, // cyclops
  {  1,  0,  2,  0,  0,  1  }, // koborokuru
  {  4, -1,  3,  0, -2,  0  }, // fomori
  {  2,  0,  1,  0,  0,  1  }, // menehune
  {  2,  0,  2, -1,  0,  0  }, // hobgoblin
  {  5, -1,  5, -2, -2,  0  }, // giant
  {  1,  0,  1,  0,  0,  2  }, // gnome
  {  2,  0,  2, -1, -1,  1  }, // oni
  {  0,  0,  0,  2,  0,  1  }, // wakyambi
  {  3,  0,  2,  0, -1,  0  }, // ogre
  {  4, -1,  3, -1, -1,  0  }, // minotaur
  {  3, -1,  2, -1, -1,  1  }, // satyr
  {  0,  2,  0,  2,  0,  0  }  // night one
};

const char *attributes[] =
  {
    "Body",
    "Quickness",
    "Strength",
    "Charisma",
    "Intelligence",
    "Willpower",
    "Reaction"
  };

const char *short_attributes[] =
{
  "bod",
  "qui",
  "str",
  "cha",
  "int",
  "wil",
  "rea"
};

struct spell_types spells[] =
  {
    // name, category, vector, target, duration, drainpower, draindamage
    { "UNDEF", 0, 0, 0, 0, 0, 0, 0 },
    { "Death Touch", FALSE, COMBAT, TOUCH, ATT_WIL, INSTANT, 0, PACK_VARIABLE_DRAIN_DAMAGE(-1) },
    { "Manabolt", FALSE, COMBAT, SINGLE, ATT_WIL, INSTANT, 0, PACK_VARIABLE_DRAIN_DAMAGE(0) },
    { "Manaball", FALSE, COMBAT, AREA, ATT_WIL, INSTANT, 0, PACK_VARIABLE_DRAIN_DAMAGE(1) },
    { "Powerbolt", TRUE, COMBAT, SINGLE, ATT_BOD, INSTANT, 1, PACK_VARIABLE_DRAIN_DAMAGE(0) },
    { "Powerball", TRUE, COMBAT, AREA, ATT_BOD, INSTANT, 1, PACK_VARIABLE_DRAIN_DAMAGE(1) },
    { "Stunbolt", FALSE, COMBAT, SINGLE, ATT_WIL, INSTANT, -1, PACK_VARIABLE_DRAIN_DAMAGE(0) },
    { "Stunball", FALSE, COMBAT, AREA, ATT_WIL, INSTANT, -1, PACK_VARIABLE_DRAIN_DAMAGE(1) },
    { "Analyze Device", TRUE, DETECTION, SINGLE, -1, SUSTAINED, 1, MODERATE },
    { "Clairaudience", FALSE, DETECTION, SINGLE, -6, SUSTAINED, 0, MODERATE },
    { "Clairvoyance", FALSE, DETECTION, SINGLE, -6, SUSTAINED, 0, MODERATE },
    { "Combat Sense", FALSE, DETECTION, SINGLE, -4, SUSTAINED, 0, SERIOUS },
    { "Detect Enemies", FALSE, DETECTION, AREA, -1, SUSTAINED, 0, MODERATE },
    { "Detect Individual", FALSE, DETECTION, SINGLE, -1, SUSTAINED, 0, LIGHT },
    { "Detect Life", FALSE, DETECTION, AREA, -1, SUSTAINED, 0, LIGHT },
    { "Detect Magic", FALSE, DETECTION, AREA, -1, SUSTAINED, 0, LIGHT },
    { "Detect Object", TRUE, DETECTION, AREA, -1, SUSTAINED, 1, MODERATE },
    { "Mindlink", FALSE, DETECTION, SINGLE, -1, SUSTAINED, 0, SERIOUS },
    { "Decrease ", FALSE, HEALTH, SINGLE, -1, SUSTAINED, 1, SERIOUS }, // Decrease Attribute
    { "Decrease Cybered ", TRUE, HEALTH, SINGLE, -1, SUSTAINED, 2, SERIOUS }, // Decrease Cybered Attribute
    { "Detox", FALSE, HEALTH, SINGLE, -1, PERMANENT, -2, LIGHT }, // drain: -2 (toxin damage) (toxin damage portion not implemented yet, so we go with Light)
    { "Heal", FALSE, HEALTH, SINGLE, -1, PERMANENT, 0, PACK_VARIABLE_DRAIN_DAMAGE(0) },
    { "Treat", FALSE, HEALTH, SINGLE, -1, PERMANENT, -1, PACK_VARIABLE_DRAIN_DAMAGE(0) },
    { "Healthy Glow", FALSE, HEALTH, SINGLE, -1, PERMANENT, 0, LIGHT },
    { "Increase ", FALSE, HEALTH, SINGLE, -1, SUSTAINED, 1, MODERATE }, // Increase Attribute
    { "Increase Cybered ", TRUE, HEALTH, SINGLE, -1, SUSTAINED, 2, MODERATE }, // Increase Cybered Attribute
    { "Increase Reaction", FALSE, HEALTH, SINGLE, ATT_REA, SUSTAINED, 1, SERIOUS },
    { "Increase Reflexes +1", FALSE, HEALTH, SINGLE, ATT_REA, SUSTAINED, 1, SERIOUS },
    { "Increase Reflexes +2", FALSE, HEALTH, SINGLE, ATT_REA, SUSTAINED, 1, DEADLY },
    { "Increase Reflexes +3", FALSE, HEALTH, SINGLE, ATT_REA, SUSTAINED, 3, DEADLY },
    { "Prophylaxis", FALSE, HEALTH, SINGLE, -1, SUSTAINED, 1, LIGHT },
    { "Resist Pain", FALSE, HEALTH, SINGLE, -1, SUSTAINED, -2, PACK_VARIABLE_DRAIN_DAMAGE(0) },
    { "Stabilize", FALSE, HEALTH, SINGLE, -1, PERMANENT, 1, MODERATE },
    { "Confusion", FALSE, ILLUSION, SINGLE, ATT_WIL, SUSTAINED, 0, SERIOUS },
    { "Mass Confusion", FALSE, ILLUSION, AREA, ATT_WIL, SUSTAINED, 0, DEADLY },
    { "Chaos", TRUE, ILLUSION, SINGLE, ATT_INT, SUSTAINED, 1, SERIOUS },
    { "Chaotic World", TRUE, ILLUSION, AREA, ATT_INT, SUSTAINED, 1, DEADLY },
    { "Invisibility", FALSE, ILLUSION, SINGLE, -1, SUSTAINED, 0, MODERATE },
    { "Improved Invisibility", TRUE, ILLUSION, SINGLE, -1, SUSTAINED, 1, MODERATE },
    { "Mask", FALSE, ILLUSION, SINGLE, -1, SUSTAINED, 0, MODERATE },
    { "Physical Mask", TRUE, ILLUSION, SINGLE, -1, SUSTAINED, 1, MODERATE },
    { "Silence", TRUE, ILLUSION, AREA, -1, SUSTAINED, 1, SERIOUS },
    { "Stealth", TRUE, ILLUSION, SINGLE, -1, SUSTAINED, 1, MODERATE },
    { "Acid Stream", TRUE, MANIPULATION, SINGLE, -1, INSTANT, 1, PACK_VARIABLE_DRAIN_DAMAGE(1) },
    { "Toxic Wave", TRUE, MANIPULATION, AREA, -1, INSTANT, 1, PACK_VARIABLE_DRAIN_DAMAGE(2) },
    { "Flamethrower", TRUE, MANIPULATION, SINGLE, -1, INSTANT, 1, PACK_VARIABLE_DRAIN_DAMAGE(1) },
    { "Fireball", TRUE, MANIPULATION, AREA, -1, INSTANT, 1, PACK_VARIABLE_DRAIN_DAMAGE(2) },
    { "Lightning Bolt", TRUE, MANIPULATION, SINGLE, -1, INSTANT, 1, PACK_VARIABLE_DRAIN_DAMAGE(1) },
    { "Ball Lightning", TRUE, MANIPULATION, AREA, -1, INSTANT, 1, PACK_VARIABLE_DRAIN_DAMAGE(2) },
    { "Clout", TRUE, MANIPULATION, SINGLE, -1, INSTANT, 0, PACK_VARIABLE_DRAIN_DAMAGE(0) },
    { "Poltergeist", TRUE, MANIPULATION, SINGLE, -1, SUSTAINED, 1, MODERATE },
    { "Armor", TRUE, MANIPULATION, SINGLE, -1, SUSTAINED, 2, MODERATE },
    { "Physical Barrier", TRUE, MANIPULATION, SINGLE, -1, SUSTAINED, 2, SERIOUS },
    { "Astral Barrier", FALSE, MANIPULATION, SINGLE, -1, SUSTAINED, 1, SERIOUS },
    { "Ice Sheet", TRUE, MANIPULATION, SINGLE, -1, INSTANT, 1, SERIOUS },
    { "Ignite", TRUE, MANIPULATION, SINGLE, -1, PERMANENT, 1, DEADLY },
    { "Light", TRUE, MANIPULATION, SINGLE, -1, SUSTAINED, 2, MODERATE },
    { "Shadow", TRUE, MANIPULATION, SINGLE, -1, SUSTAINED, 2, MODERATE },
    { "Laser", TRUE, MANIPULATION, SINGLE, -1, INSTANT, 1, PACK_VARIABLE_DRAIN_DAMAGE(1) },
    { "Nova", TRUE, MANIPULATION, AREA, -1, INSTANT, 1, PACK_VARIABLE_DRAIN_DAMAGE(2) },
    { "Steam", TRUE, MANIPULATION, SINGLE, -1, INSTANT, 0, PACK_VARIABLE_DRAIN_DAMAGE(1) },
    { "Smoke Cloud", TRUE, MANIPULATION, AREA, -1, INSTANT, 1, PACK_VARIABLE_DRAIN_DAMAGE(2) },
    { "Thunderbolt", TRUE, MANIPULATION, SINGLE, -1, INSTANT, 0, PACK_VARIABLE_DRAIN_DAMAGE(1) },
    { "Thunderclap", TRUE, MANIPULATION, AREA, -1, INSTANT, 1, PACK_VARIABLE_DRAIN_DAMAGE(2) },
    { "Waterbolt", TRUE, MANIPULATION, SINGLE, -1, INSTANT, 0, PACK_VARIABLE_DRAIN_DAMAGE(1) }, // these were both VDD+0, but I can't find them in the book, so I'm matching them to the other elemental manips. -LS
    { "Splash", TRUE, MANIPULATION, AREA, -1, INSTANT, 0, PACK_VARIABLE_DRAIN_DAMAGE(2) },
    { "Nightvision", TRUE, DETECTION, SINGLE, -1, SUSTAINED, 1, MODERATE },
    { "Infravision", TRUE, DETECTION, SINGLE, -1, SUSTAINED, 1, MODERATE },
    { "Levitate", TRUE, MANIPULATION, SINGLE, -1, SUSTAINED, 2, MODERATE },
    { "Flame Aura", TRUE, MANIPULATION, SINGLE, -1, SUSTAINED, 2, MODERATE }
  };

const char *totem_types[] =
  {
    "UNDEFINED",
    "Bear",
    "Buffalo",
    "Cat",
    "Coyote",
    "Dog",
    "Dolphin",
    "Eagle",
    "Gator",
    "Lion",
    "Mouse",
    "Owl",
    "Raccoon",
    "Rat",
    "Raven",
    "Shark",
    "Snake",
    "Wolf",
    "Badger",
    "Bat",
    "Boar",
    "Bull",
    "Cheetah",
    "Cobra",
    "Crab",
    "Crocodile",
    "Dove",
    "Elk",
    "Fish",
    "Fox",
    "Gecko",
    "Goose",
    "Horse",
    "Hyena",
    "Jackal",
    "Jaguar",
    "Leopard",
    "Lizard",
    "Monkey",
    "Otter",
    "Parrot",
    "Polecat",
    "Prairie dog",
    "Puma",
    "Python",
    "Scorpion",
    "Spider",
    "Stag",
    "Turtle",
    "Whale",
    "Sun",
    "Lover",
    "Seductress",
    "Siren",
    "Oak",
    "Moon",
    "Mountain",
    "Sea",
    "Stream",
    "Wind",
    "Adversary",
    "Bacchus",
    "Creator",
    "Dark King",
    "Dragonslayer",
    "Fire Bringer",
    "Horned Man",
    "Sea King",
    "Sky Father",
    "Wild Huntsman",
    "Wise Warrior",
    "Great Mother",
    "Moon Maiden",
    "Trickster"
  };

const char *spell_category[6] =
  {
    "UNDEF",
    "Combat",
    "Detection",
    "Health",
    "Illusion",
    "Manipulation"
  };

const char *durations[3] =
  {
    "Instant",
    "Sustained",
    "Permanent"
  };

struct spirit_table elements[4] =
  {
    {"Earth", 26
    },
    {"Fire", 27},
    {"Air", 25},
    {"Water", 28}
  };

const char *foci_type[] =
  {
    "expendable",
    "specific spell",
    "category",
    "spirit",
    "power",
    "sustaining",
    "weapon",
    "defensive"
  };

const char *ranges[4] =
  {
    "Single",
    "Area",
    "Touch",
    "Caster Only"
  };
const char *light_levels[] =
  {
    "Normal",
    "Normal (No City Street Lights)",
    "Completely Dark",
    "Minimal Light",
    "Partly Lit",
    "Glare",
    "Mist",
    "Light Smoke/Fog",
    "Heavy Smoke/Fog",
    "Thermal Smoke"
  };

const char *room_types[] =
  {
    "City Street",
    "Suburban Street",
    "Highway",
    "Field",
    "Forest",
    "Indoors (Shop)",
    "Indoors (Warehouse)",
    "Indoors (Club)",
    "Indoors (Restaurant/Bar)",
    "Indoors (Office)",
    "Indoors (House)"
  };

const char *cyber_grades[4] =
{
  "Standard",
  "Alpha",
  "Beta",
  "Delta"
};

const char *vision_types[] = {
  "Normal",
  "Low-Light",
  "Thermographic",
  "Ultrasonic"
};

const char *vision_bits[] = {
  "natural",
  "implanted",
  "equipment",
  "optical",
  "electronic",
  "adept-power",
  "spell-given",
  "overridden"
};

const char *eyemods[] = {
  "Camera",
  "Cyber Replacement (MUST SET FOR PACKAGES)",
  "Display Link",
  "Flare Compensation",
  "Image Link",
  "Low Light",
  "Protective Covers",
  "Clock",
  "Retinal Duplication",
  "Thermographic",
  "Optical Magnification 1",
  "Optical Magnification 2",
  "Optical Magnification 3",
  "Electronic Magnification 1",
  "Electronic Magnification 2",
  "Electronic Magnification 3",
  "Eye Gun",
  "Datajack",
  "Dart",
  "Laser",
  "Light",
  "Ultrasound",
  "Cosmetic",
  "\n"
};

const char *armsmods[] = {
  "Obvious",
  "Synthetic",
  "Armor Mod1",
  "Strength Mod1",
  "Strength Mod2",
  "Strength Mod3",
  "Quickness Mod1",
  "Quickness Mod2",
  "Quickness Mod3",
  "Cyberarm Gyromount",
  "\n"
};

const char *legsmods[] = {
  "Obvious",
  "Synthetic",
  "Armor Mod1",
  "Strength Mod1",
  "Strength Mod2",
  "Strength Mod3",
  "Quickness Mod1",
  "Quickness Mod2",
  "Quickness Mod3",
  "Kid Stealth",
  "\n"
};

const char *skullmods[] = {
  "Obvious",
  "Synthetic",
  "Armor Mod1",
  "Tac Comp",
  "\n"
};

const char *torsomods[] = {
  "Obvious",
  "Synthetic",
  "Armor Mod1",
  "Armor Mod2",
  "Armor Mod3",
  "\n"
};

const char *decap_cyber_grades[4] =
{
  "standard",
  "alpha",
  "beta",
  "delta"
};

const char *bone_lacing[5] =
{
   "Plastic",
   "Aluminium",
   "Titanium",
   "Kevlar",
   "Ceramic"
};

const char *hand_razors[3] = {
  "Retractable",
  "Improved",
  "\n"
};
const char *cyber_types[] = {
  "Chipjack",
  "Datajack",
  "Datalock",
  "Knowsoft Link",
  "Memory",
  "Tooth Compartment",
  "Phone",
  "Radio",
  "Eye Mods",
  "Bone Lacing",
  "Fingertip Compartment",
  "Hand Blade",
  "Hand Razor",
  "Hand Spur",
  "Voice Modulator",
  "Boosted Reflexes",
  "Dermal Plating",
  "Filtration",
  "Simrig",
  "Skillwires",
  "Vehicle Control Rig",
  "Wired Reflexes",
  "Reaction Enhancer",
  "Reflex Trigger",
  "Balance Augmenter",
  "Oral Dart",
  "Oral Gun",
  "Oral Slasher",
  "ASIST Interface",
  "Chipjack Expert Driver",
  "Cranial Cyberdeck",
  "Data Compactor",
  "Encephalon",
  "Math SPU",
  "Auto Injector",
  "Balance Tail",
  "Body Compartment",
  "Cyber Fin",
  "Cyber Skull",
  "Cyber Torso",
  "Dermal Sheathing",
  "Foot Anchor",
  "Hydraulic Jack",
  "Move By Wire",
  "Climbing Claws",
  "Smartlink",
  "Muscle Replacement",
  "Paired Set of Cyber Arms",
  "Paired Set of Cyber Legs",
  "Tactical Computer",
  "Custom NERPS Cyberware",
  "Cranial Remote Deck",
  "Cyber Fangs",
  "Horn Implants"
};

const char *decap_cyber_types[] = {
  "chipjack",
  "datajack",
  "datalock",
  "knowsoft link",
  "memory chip",
  "tooth compartment",
  "headware phone",
  "headware radio",
  "eye modification",
  "bone lacing",
  "fingertip compartment",
  "hand blade",
  "hand razor",
  "hand spur",
  "voice modulator",
  "boosted reflexes",
  "dermal plating",
  "filtration",
  "simrig",
  "skillwires",
  "vehicle control rig",
  "wired reflexes",
  "reaction enhancer",
  "reflex trigger",
  "balance augmenter",
  "oral dart",
  "oral gun",
  "oral slasher",
  "ASIST interface",
  "chipjack expert driver",
  "cranial cyberdeck",
  "data compactor",
  "encephalon",
  "math SPU",
  "auto injector",
  "balance tail",
  "body compartment",
  "cyber fin",
  "cyber skull",
  "cyber torso",
  "dermal sheathing",
  "foot anchor",
  "hydraulic jack",
  "move by wire",
  "climbing claws",
  "smartlink",
  "muscle replacement",
  "paired set of cyber arms",
  "paired set of cyber legs",
  "tactical computer",
  "custom NERPS cyberware",
  "cranial remote deck",
  "cyber fangs",
  "horn implants"
};

const char *bio_types[] = {
  "Adrenal Pump",
  "Cat's Eyes",
  "Digestive Expansion",
  "Enhanced Articulation",
  "Extended Volume",
  "Metabolic Arrestor",
  "Muscle Augmentation",
  "Muscle Toner",
  "Nephritic Screen",
  "Nictitating Gland",
  "Orthoskin",
  "Pathogenic Defense",
  "Platelet Factory",
  "Suprathyroid Gland",
  "Symbiotes",
  "Synthacardium",
  "Tailored Pheromones",
  "Toxin Extractor",
  "Tracheal Filter",
  "Cerebral Booster",
  "Damage Compensator",
  "Pain Editor",
  "Reflex Recorder",
  "Synaptic Accelerator",
  "Thermosense Organs",
  "Trauma Damper",
  "Custom NERPS Bioware",
  "Erythropoitin",
  "Calcitonin",
  "Phenotypic Alteration (Bod)",
  "Phenotypic Alteration (Qui)",
  "Phenotypic Alteration (Str)"
};

const char *decap_bio_types[] = {
  "adrenal pump",
  "pair of cat's eyes",
  "digestive expansion",
  "enhanced articulation",
  "extended volume",
  "metabolic arrestor",
  "muscle augmentation",
  "muscle toner",
  "nephritic screen",
  "nictitating gland",
  "orthoskin",
  "pathogenic defense",
  "platelet factory",
  "suprathyroid gland",
  "symbiotes",
  "synthacardium",
  "infusion of tailored pheromones",
  "toxin extractor",
  "tracheal filter",
  "cerebral booster",
  "damage compensator",
  "pain editor",
  "reflex recorder",
  "synaptic accelerator",
  "thermosense organs",
  "trauma damper",
  "custom NERPS bioware",
  "erythropoitin",
  "calcitonin",
  "phenotypic alteration (bod)",
  "phenotypic alteration (qui)",
  "phenotypic alteration (str)"
};

const char *metamagic[] = {
  "UNDEFINED",
  "Centering",
  "Cleansing",
  "Invoking",
  "Masking",
  "Possessing",
  "Quickening",
  "Reflecting",
  "Shielding",
  "Anchoring"
};

const char *legality_codes[][2] = {
  { "*", "Nothing" },
  { "A", "Small Blade" },
  { "B", "Large Blade" },
  { "C", "Blunt Weapon" },
  { "D", "Projectile" },
  { "E", "Pistol" },
  { "F", "Rifle" },
  { "G", "Automatic Weapon" },
  { "H", "Heavy Weapon" },
  { "J", "Explosive" },
  { "K", "Military Weapon" },
  { "L", "Military Armor" },
  { "M", "Military Ammunition" },
  { "N", "Class A Cyberware" },
  { "Q", "Class B Cyberware" },
  { "R", "Class C Cyberware" },
  { "S", "Class D Matrix" },
  { "T", "Class E Magic" },
  { "U", "Class A Equipment" },
  { "V", "Class B Equipment" },
  { "W", "Class C Equipment" },
  { "X", "Class A Controlled" },
  { "Y", "Class B Controlled" },
  { "Z", "Class C Controlled" }
};

const char *background_types[] =
{
  "violence",
  "torture",
  "hatred",
  "genocide",
  "ritual magic",
  "sacrifice",               // 5
  "worship",
  "death",
  "pollution",
  "concentrated magic",
  "lost humanity",           // 10
  "sterility",
  "confusion",
  "power site",
  "blood magic",
  "violence",                   // 15
  "death",
  "ERROR"
};

const char *fire_mode[] =
{
  "NOTHING",
  "Single Shot",
  "Semi-Automatic",
  "Burst Fire",
  "Full Automatic",
  "ERROR"
};

const char *combat_modifiers[] =
{
  "Recoil",
  "Movement",
  "Dual Wielding",
  "Smartlink",
  "Distance",
  "Visibility",
  "Position",
  "Gyro Recoil Comp",
  "Net Reach",
  "VehDamaged",
  "Defender Moving",
  "In Melee Combat",
  "Opponent Burst",
  "Foot Anchors",
  "Vehicle Handling",
  "Non-Weapon",
  "ERROR"
};

struct pgroup_priv_struct pgroup_privileges[] =
{
  {"Administrator" , TRUE  , "Administrators can grant any privilege that they possess. They can also promote and demote anyone below their level. Only the Leader may assign the Administrator privilege." },
  {"Auditor"       , TRUE  , "Auditors can see the pgroup's logs." },
  {"Architect"     , FALSE , "Architects can alter the PGHQ's blueprints." },
  {"Co-Conspirator", TRUE  , "Co-Conspirators pierce the veil of secrecy and see unredacted logs even in secret orgs. No effect in non-secret orgs." },
  {"Driver"        , FALSE , "Drivers are authorized to use PG vehicles." },
  {"Landlord"      , FALSE , "Landlords are authorized to modify PG housing, including leasing and releasing rooms." },
  {"Leader"        , TRUE  , "The Leader of the group has all permissions. There can be only one leader. Power is transferred through abdication and contesting." },
  {"Mechanic"      , FALSE , "Mechanics can install and remove components from PG vehicles." },
  {"Procurer"      , FALSE , "Procurers can purchase things with PG funds." },
  {"Recruiter"     , TRUE  , "Recruiters can send invitations to join the PG." },
  {"Tenant"        , FALSE , "Tenants may enter all PG apartments." },
  {"Treasurer"     , TRUE  , "Treasurers may see the balance of the PG account, and can wire money from it." }
};

const char *workshops[] = {
  "General",
  "Electronics",
  "Microtronics",
  "Cyberware",
  "Vehicle",
  "Weaponry",
  "Medical",
  "Ammunition",
  "Gunsmithing"
};

const char *kit_workshop_facility[] = {
  "ERROR",
  "kit",
  "workshop",
  "facility"
};

const char *projectile_ammo_types[] = {
  "Arrow",
  "Bolt",
  "Shuriken",
  "Throwing Knife"
};

const char *magic_tool_types[] = {
  "Conjuring Library",
  "Sorcery Library",
  "Hermetic Circle",
  "Shamanic Lodge",
  "Summoning Materials"
};

const char *holster_types[] = {
  "Pistol / SMG",
  "Sword",
  "Rifle (or larger)"
};

const char *docwagon_contract_types[] = {
  "Bugged (alert an imm)", // undefined / 0
  "Basic",
  "Gold",
  "Platinum"
};

const char *gun_accessory_locations[] = {
  "top",
  "barrel",
  "underside"
};

const char *gun_accessory_types[] = {
  "Undefined",
  "Smartlink",
  "Scope",
  "Gas Vent",
  "Shock Pad",
  "Silencer",
  "Sound Suppressor",
  "Smart Goggles",
  "Bipod",
  "Tripod",
  "Bayonet"
};

const char *mount_types[] = {
  "Firmpoint Internal Fixed Mount",
  "Firmpoint External Fixed Mount",
  "Hardpoint Internal Fixed Mount",
  "Hardpoint External Fixed Mount",
  "Turret",
  "Mini-Turret"
};

const char *deck_accessory_upgrade_types[] = {
  "MPCP (replacement)",
  "Active Memory",
  "Storage Memory",
  "Hitcher Jack",
  "I/O Speed",
  "Reaction Increase"
};

const char *message_history_channels[] = {
  "Hired",
  "Newbie",
  "OOC",
  "OSays",
  "Pages",
  "Phone",
  "RPE",
  "Radio",
  "Says",
  "Shouts",
  "Tells",
  "Wiztells",
  "Emotes",
  "Local"
};

const char *pgroup_settings[] = {
  "Founded",
  "Disabled",
  "Clandestine",
  "Cloned / temporary"
};

int acceptable_weapon_attachment_affects[] = {
  AFF_LASER_SIGHT,
  AFF_VISION_MAG_1,
  AFF_VISION_MAG_2,
  AFF_VISION_MAG_3,
  AFF_INFRAVISION,
  AFF_LOW_LIGHT,
  AFF_ULTRASOUND,
  -1
};

const char *aspect_names[] = {
  "Full Mage",
  "Conjurer",
  "Shamanist",
  "Sorcerer",
  "Earth Elementalist",
  "Air Elementalist",
  "Fire Elementalist",
  "Water Elementalist",
  "Earth Mage",
  "Air Mage",
  "Fire Mage",
  "Water Mage"
};

const char *tradition_names[] = {
  "Hermetic",
  "Shamanic",
  "Mundane",
  "UNKNOWN",
  "Adept",
  "ERROR"
};

const char *damage_type_names_must_subtract_300_first_and_must_not_be_greater_than_blackic[] = {
  "Hit",
  "Sting",
  "Whip",
  "Slash",
  "Bite",
  "Bludgeon",
  "Crush",
  "Pound",
  "Claw",
  "Maul",
  "Thrash",
  "Pierce",
  "Punch",
  "Stab",
  "Taser",
  "Shuriken",
  "Throwing Knife",
  "Arrow",
  "Hand Grenade",
  "Grenade Launcher",
  "Rocket",
  "Pistol",
  "Blast",
  "Rifle",
  "Shotgun",
  "Machine Gun",
  "Cannon",
  "Bifurcate",
  "Crash",
  "Dumpshock",
  "Blackic"
};

const char *pc_race_types[] =
  {
    "Undef",
    "Undefined",
    "Human",
    "Dwarf",
    "Elf",
    "Ork",
    "Troll",
    "Cyclops",
    "Koborokuru",
    "Fomori",
    "Menehune",
    "Hobgoblin",
    "Giant",
    "Gnome",
    "Oni",
    "Wakyambi",
    "Ogre",
    "Minotaur",
    "Satyr",
    "Night-One",
    "Dryad",
    "Dragon",
    "Elemental",
    "Spirit",
    "Conjured Elemental",
    "\n"
  };

const char *pc_race_types_decap[] =
  {
    "undef",
    "undefined",
    "human",
    "dwarf",
    "elf",
    "ork",
    "troll",
    "cyclops",
    "koborokuru",
    "fomori",
    "menehune",
    "hobgoblin",
    "giant",
    "gnome",
    "oni",
    "wakyambi",
    "ogre",
    "minotaur",
    "satyr",
    "night-One",
    "dryad",
    "dragon",
    "elemental",
    "spirit",
    "conjured elemental",
    "\n"
  };

struct nuyen_faucet_or_sink nuyen_faucets_and_sinks[NUM_OF_TRACKED_NUYEN_INCOME_SOURCES] =
  {
    {"Decking (paydata)", NI_IS_FAUCET},
    {"Shop Sales", NI_IS_FAUCET},
    {"Autoruns", NI_IS_FAUCET},
    {"NPC Loot", NI_IS_FAUCET},
    {"Shop Purchases", NI_IS_SINK},
    {"Spec Proc (generic)", NI_IS_SINK},
    {"Vehicle Purchase", NI_IS_SINK},
    {"'Ware Install'", NI_IS_SINK},
    {"Initiation", NI_IS_SINK},
    {"Housing", NI_IS_SINK},
    {"Training (metamagic)", NI_IS_SINK},
    {"Training (stat)", NI_IS_SINK},
    {"Training (skill)", NI_IS_SINK},
    {"Training (spell)", NI_IS_SINK},
    {"Ammo Building", NI_IS_SINK},
    {"Repairs", NI_IS_SINK},
    {"Where's My Car", NI_IS_SINK},
    {"Junking", NI_IS_FAUCET},
    {"Trade Command (k->n)", NI_IS_FAUCET},
    {"Idle Reward", NI_IS_FAUCET},
    {"Docwagon", NI_IS_SINK},
    {"Lodge/Circle Cost", NI_IS_SINK},
    {"Taxis", NI_IS_SINK},
    {"Playergroups", NI_IS_SINK},
    {"Trade Command (n->k)", NI_IS_SINK},
    {"Credstick Cracker", NI_IS_SINK},
    {"Death Penalty", NI_IS_SINK},
    {"Drug Withdrawal / Fugue", NI_IS_SINK},
    {"Ritual Casting", NI_IS_SINK},
    {"Staff Payout", NI_IS_FAUCET},
    {"Staff Charge", NI_IS_SINK},
    {"Syspoint Purchase", NI_IS_SINK}
  };

const char *ignored_bits_in_english[] =
  {
    "where",
    "oocs",
    "radio",
    "mindlinks",
    "tells",
    "osays",
    "interaction",
    "following",
    "calls",
    "ERROR: Coder forgot to add their new bit to ignored_bits_in_english!"
  };

const char *mtx_subsystem_names[] = {
  "access",
  "control",
  "index",
  "files",
  "slave"
};

const char *veh_speeds[] = {
  "off",
  "idle",
  "cruising",
  "speeding",
  "max"
};
