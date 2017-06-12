#include "structs.h"
#include "awake.h"

const char *awakemud_version[] =
    {
      "AwakeMUD Community Edition, version 0.9.1 BETA\r\n"
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
    "almost out of sight"
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
    "SENATE",
    "!QUIT",
    "SENT",
    "ASTRAL",
    "!GRID",
    "STORAGE",
    "\n"
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
    "\n"
  };


/* SEX_x */
const char *genders[] =
  {
    "Neutral",
    "Male",
    "Female",
    "\n"
  };

const char *thrdgenders[] =
  {
    "it",
    "him",
    "her",
    "\n"
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

const char *weapon_type[] =
  {
    "edged weapon",
    "club",
    "pole arm",
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
    "!USED",
    "SITEOK",
    "!SHOUT",
    "!TITLE",
    "DELETED",
    "!DELETE",
    "PACKING",
    "!STAT",
    "LOADRM",
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
    "\n"
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
    "C1",
    "C2",
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
    "\n"
  };

/* AFF_x */
const char *affected_bits[] =
  {
    "NOTHING",
    "Ruthenium",
    "Banishing",
    "Det-invis",
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
    "Petrify",
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
    "Withdrawl (Forced)",
    "Withdrawl",
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
    "Ammo Building"
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
    "<patch>              "
  };


const char *hands[] =
  {
    "<right hand>         ",
    "<left hand>          ",
    "<both hands>         ",
  };

const char *wielding_hands[] =
  {
    "<right hand (w)>     ",
    "<left hand (w)>      ",
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
    "PATCH"
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
    "ARM",
    "FACE",
    "THIGH",
    "PATCH",
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
    {"Attack", 0 },
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
    {"Armour", 3 },
    {"Camo", 3 },
    {"Crash", 3 },
    {"Defuse", 2 },
    {"Evaluate", 2 },
    {"Validate", 4 },
    {"Swerve", 3 },
    {"Suite", 15 },
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
    {"Response Increase", 0 }
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
    { "Hardening", TYPE_SHOP, SOFT_HARDENING, 3 },
    { "ICCM Biofeedback Filter", TYPE_SHOP, SOFT_ICCM, 1 },
    { "Icon Chip", TYPE_KIT, SOFT_ICON, 0 },
    { "I/O Speed", TYPE_KIT, 0, 0 },
    { "MPCP", TYPE_SHOP, SOFT_MPCP, 0 },
    { "Bod Chip", TYPE_KIT, SOFT_BOD, 0 },
    { "Sensor Chip", TYPE_KIT, SOFT_SENSOR, 0 },
    { "Masking Chip", TYPE_KIT, SOFT_MASKING, 1 },
    { "Evasion Chip", TYPE_KIT, SOFT_EVASION, 1 },
    { "Ports (FUPS)", TYPE_KIT, 0, -1 },
    { "RAS Override", TYPE_KIT, 0, 0 },
    { "Reality Filters", TYPE_SHOP, SOFT_REALITY, 2 },
    { "Response Increase", TYPE_SHOP, SOFT_RESPONSE, 1 },
    { "Matrix Interface", TYPE_KIT, 0, -1 },
    { "Maser Interface", TYPE_KIT, 0, 0 },
    { "Cellular Interface", TYPE_KIT, 0, 1 },
    { "Laser Interface", TYPE_KIT, 0, 0 },
    { "Microwave Interface", TYPE_SHOP, 0, 1},
    { "Radio Interface", TYPE_KIT, 0, 0 },
    { "Satellite Interface", TYPE_SHOP, 0, 2 },
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
                       {8, 0, 1},      // Vodka
                       {9, 0, 0},      // Tequila
                       {7, 0, -1},      // Brandy
                       {8, 0, 0},      // Rum
                       {8, 0, -1},      // Whiskey
                       {7, 0, 1},      // Gin
                       {5, 0, 1},      // Champagne
                       {4, 1, 2},      // Red Wine
                       {4, 1, 2},      // White Wine
                       {0, 2, -1},     // Blood
                       {0, 1, -1},     // Hot Sauce
                       {0, 1, -4},     // Urine
                       {6, 0, -1},     // Local
                       {12, 0, -1}     // Fuckup Juice
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
    "dark amber",    //Local
    "thick"          //fuckup juice
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
    "None",
    "Astral Perception",
    "Combat Sense",
    "Blind Fighting",
    "Quick Strike",
    "Killing Hands",
    "Nerve Strike",
    "Smashing Blow",
    "Distance Strike",
    "Increased Reflexes",
    "Boosted Strength",
    "Boosted Quickness",
    "Boosted Body",
    "Improved Strength",
    "Improved Quickness",
    "Improved Body",
    "Improved Perception",
    "Improved Sense (Low Light)",
    "Improved Sense (Thermographic)",
    "Improved Sense (Magnification)",
    "Magic Resistance",
    "Pain Resistance",
    "Temperature Tolerance",
    "Spell Shroud",
    "True Sight",
    "Missile Parry",
    "Missile Mastery",
    "Mystic Armour",
    "Rapid Healing",
    "Freefall",
    "Iron Will",
    "Aid Spell",
    "Empathic Healing",
    "Kinesics",
    "Linguistics",
    "Living Focus",
    "Motion Sense",
    "Pain Relief",
    "Penetrating Strike",
    "Side Step",
    "Sustenance",
    "Counterstrike"
  };

struct skill_data skills[] =
  {
    {"OMGWTFBBQ", BOD, 0},
    {"athletics", BOD, 0},
    {"armed combat", STR, 0},
    {"edged weapons", STR, 0},
    {"pole arms", STR, 0},
    {"whips & flails", QUI, 0},
    {"clubs", STR, 0},
    {"brawling", STR, 0},
    {"centering", WIL, 0},
    {"cyber implants", STR, 0},
    {"firearms", STR, 0},
    {"pistols", QUI, 0},
    {"rifles", QUI, 0},
    {"shotguns", QUI, 0},
    {"assault rifles", QUI, 0},
    {"sub-machine guns", QUI, 0},
    {"grenade launchers", INT, 0},
    {"tasers", QUI, 0},
    {"gunnery", INT, 0},
    {"machine guns", STR, 0},
    {"missile launchers", INT, 0},
    {"assault cannons", STR, 0},
    {"artillery", STR, 0},
    {"projectiles", STR, 0},
    {"oral strike", QUI, 0},
    {"spell design", INT, 1},
    {"throwing weapons", STR, 0},
    {"enchanting", WIL, 0},
    {"cyberterminal design", INT, 1},
    {"demolitions", INT, 0},
    {"computers", INT, 0},
    {"electronics", INT, 0},
    {"computer B/R", INT, 0},
    {"biotech", INT, 0},
    {"electronics B/R", INT, 0},
    {"talismongering", INT, 1},
    {"police procedures", INT, 1},
    {"leadership", CHA, 0},
    {"interrogation", CHA, 0},
    {"negotiation", CHA, 0},
    {"chanting", CHA, 0},
    {"conjuring", WIL, 0},
    {"sorcery", WIL, 0},
    {"legerdemain", QUI, 0},
    {"corporate etiquette", CHA, 0},
    {"media etiquette", CHA, 0},
    {"street etiquette", CHA, 0},
    {"tribal etiquette", CHA, 0},
    {"elf etiquette", CHA, 0},
    {"program design (Combat)", INT, 1},
    {"program design (Defensive)", INT, 1},
    {"program design (Operational)", INT, 1},
    {"program design (Special)", INT, 1},
    {"program design (Cyberterminal)", INT, 1},
    {"data brokerage", INT, 1},
    {"aura reading", INT, 0},
    {"stealth", QUI, 0},
    {"steal", 0, 0},
    {"track", 0, 0},
    {"climbing", 0, 0},
    {"driving/Bike", REA, 0},
    {"driving/Nothing", REA, 0},
    {"driving/Car", REA, 0},
    {"driving/Truck", REA, 0},
    {"BR/Bike", INT, 0},
    {"BR/Car", INT, 0},
    {"BR/Drone", INT, 0},
    {"BR/Truck", INT, 0},
    {"Dancing", QUI, 0},
    {"Singing", INT, 0},
    {"Instrument", INT, 0},
    {"Arcane language", INT, 1},
    {"Meditation", WIL, 0},
    {"English", INT, 1},
    {"Sperethiel", INT, 1},
    {"Spanish", INT, 1},
    {"Japanese", INT, 1},
    {"Chinese", INT, 1},
    {"Korean", INT, 1},
    {"Italian", INT, 1},
    {"Russian", INT, 1},
    {"Sioux", INT, 1},
    {"Makaw", INT, 1},
    {"Crow", INT, 1},
    {"Salish", INT, 1},
    {"Ute", INT, 1},
    {"Navajo", INT, 1},
    {"German", INT, 1},
    {"Or'zet", INT, 1},
    {"Arabic", INT, 1},
    {"Latin", INT, 1},
    {"Gaelic", INT, 1},
    {"French", INT, 1},
    {"Animal Handling", CHA, 0},
    {"Animal Taming", CHA, 0},
    {"Edged Weapons B/R", INT, 0},
    {"Polearms B/R", INT, 0},
    {"Clubs B/R", INT, 0},
    {"Throwing Weapons B/R", INT, 0},
    {"Whips/Flails B/R", INT, 0},
    {"Projectiles B/R", INT, 0},
    {"Pistols B/R", INT, 0},
    {"Shotguns B/R", INT, 0},
    {"Rifles B/R", INT, 0},
    {"Heavy Weapons B/R", INT, 0},
    {"Submachine Guns B/R", INT, 0},
    {"Armour B/R", INT, 0},
    {"Off-hand Edged Weapons", STR, 0},
    {"Off-hand Clubs", STR, 0},
    {"Off-hand Cyber Implants", STR, 0},
    {"Off-hand Whips/Flails", QUI, 0},
    {"Survival", INT, 0},
    {"Navigation (Land)", INT, 0},
    {"Navigation (Water)", INT, 0},
    {"Navigation (Air)", INT, 0},
    {"Small Unit Tactics", INT, 0},
    {"Chemistry", INT, 0},
    {"Diving", BOD, 0},
    {"Parachuting", BOD, 0},
    {"Underwater Combat", STR, 0},
    {"Driving/Rotorcraft", REA, 0},
    {"Driving/Fixed Wing", REA, 0},
    {"Driving/Vector Thrust", REA, 0},
    {"Acting", CHA, 0},
    {"Disguise", INT, 0},
    {"Lock Picking", QUI, 0},
    {"Riding", QUI, 0},
    {"Spray Weapons", QUI, 0},
    {"Intimidation", CHA, 0},
    {"Gun Cane", QUI, 0},
    {"Bracer Gun", QUI, 0},
    {"Blowgun", QUI, 0},
    {"Pharmacuticals", INT, 0}
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

const char *veh_type[11] =
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
    "Lighter Than Air"
  };
struct mod_data mod_types[NUM_MODTYPES] =
  {
    { "NOTHING", 0 },
    { "Engine", TYPE_FACILITY },
    { "Nitrous Oxide Injectors", TYPE_FACILITY },
    { "Turbocharger", TYPE_SHOP },
    { "Autonav", TYPE_FACILITY },
    { "CMC", TYPE_FACILITY },
    { "Drive-By-Wire", TYPE_FACILITY },
    { "Improved Suspension", TYPE_SHOP },
    { "Motorbike Gyroscope", TYPE_FACILITY },
    { "Offroad Suspension", TYPE_FACILITY },
    { "Armour", TYPE_FACILITY },
    { "Concealed Armour", TYPE_FACILITY },
    { "Crashcage", TYPE_SHOP },
    { "Rollbars", TYPE_SHOP },
    { "Thermal Masking", TYPE_FACILITY },
    { "Mounts", TYPE_FACILITY },
    { "ECM", TYPE_FACILITY },
    { "ECCM", TYPE_FACILITY },
    { "Sensors", TYPE_FACILITY },
    { "Seats", TYPE_SHOP },
    { "Tires", TYPE_KIT },
    { "Other", TYPE_KIT },
    { "Ammo Bin", TYPE_KIT }
    
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
    "Armour",
    "Seats",
    "Computer1",
    "Computer2",
    "Computer3",
    "Phone",
    "Radio",
    "Mounts"
  };

const char *engine_type[6] =
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
    "Pilot"
  };

const char *veh_flag[] =
  {
    "Nothing",
    "Can Fly",
    "Amphibious",
    "Workshop",
    "Newbie"
  };

const char *jurid[] =
  {
    "Seattle",
    "Portland",
    "Caribbean",
    "Ocean",
    "\n"
  };

const char *host_sec[] =
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
    "ARMOUR",
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
    "Armour, ",
    "Cascading Attack, ",
    "Expert Defense, ",
    "Expert Offense, ",
    "Shield, ",
    "Shift, ",
    "Trap  "
  };

struct ammo_data ammo_type[] =
  {
    {"normal", 2, 1, .25, 2},
    {"APDS", 14, 14, .25, 7},
    {"explosive", 3, 1.5, .75, 5},
    {"EX", 6, 3, .75, 10},
    {"flechette", 3, 1.5, .5, 10},
    {"gel", 4, 2, .25, 3}
  };

const char *positions[] =
  {
    " is lying here, dead",
    " is lying here, mortally wounded",
    " is lying here, stunned",
    " is sleeping here",
    " is lying here",
    " is resting here",
    " is sitting here",
    "!FIGHTING!",
    " is standing here"
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

struct drug_data drug_types[] =
  {
    { "Nothing", 0, 0, 0, 0, 0, 0, 0, 0
    },
    { "ACTH", 0, LIGHT, 0, 0, 3, 10, 0, 0 },
    { "Hyper", 6, SERIOUS, 0, 0, 0, 0, 0, 0 },
    { "Jazz", 0, 0, 4, 5, 2, 2, 8, 3 },
    { "Kamikaze", 0, 0, 0, 5, 2, 2, 10, 2 },
    { "Psyche", 0, 4, 0, 2, 10, 20, 7 },
    { "Bliss", 0, 0, 5, 5, 2, 2, 30, 2 },
    { "Burn", 3, DEADLY, 2, 0, 2, 20, 100, 1 },
    { "Cram", 0, 0, 4, 0, 2, 5, 50, 2 },
    { "Nitro", 4, DEADLY, 5, 8, 3, 2, 5, 3 },
    { "Novacoke", 0, 0, 6, 5, 2, 3, 50, 2 },
    { "Zen", 0, 0, 3, 0, 2, 5, 50, 2 }
  };

struct spell_types spells[] =
  {
    { "UNDEF", 0, 0, 0, 0, 0, 0, 0 },
    { "Death Touch", FALSE, COMBAT, TOUCH, ATT_WIL, INSTANT, 0, -4 },
    { "Manabolt", FALSE, COMBAT, SINGLE, ATT_WIL, INSTANT, 0, -3 },
    { "Manaball", FALSE, COMBAT, AREA, ATT_WIL, INSTANT, 0, -2 },
    { "Powerbolt", TRUE, COMBAT, SINGLE, ATT_BOD, INSTANT, 1, -3 },
    { "Powerball", TRUE, COMBAT, AREA, ATT_BOD, INSTANT, 1, -2 },
    { "Stunbolt", FALSE, COMBAT, SINGLE, ATT_WIL, INSTANT, -1, -3 },
    { "Stunball", FALSE, COMBAT, AREA, ATT_WIL, INSTANT, -1, -2 },
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
    { "Decrease Attribute", FALSE, HEALTH, SINGLE, -1, SUSTAINED, 1, SERIOUS },
    { "Decrease Cybered Attribute", TRUE, HEALTH, SINGLE, -1, SUSTAINED, 2, SERIOUS },
    { "Detox", FALSE, HEALTH, SINGLE, -1, PERMANENT, -2, 0 },
    { "Heal", FALSE, HEALTH, SINGLE, -1, PERMANENT, 0, -3 },
    { "Treat", FALSE, HEALTH, SINGLE, -1, PERMANENT, -1, -3 },
    { "Healthy Glow", FALSE, HEALTH, SINGLE, -1, PERMANENT, 0, LIGHT },
    { "Increase Attribute", FALSE, HEALTH, SINGLE, -1, SUSTAINED, 1, MODERATE },
    { "Increase Cybered Attribute", TRUE, HEALTH, SINGLE, -1, SUSTAINED, 2, MODERATE },
    { "Increase Reaction", FALSE, HEALTH, SINGLE, ATT_REA, SUSTAINED, 1, SERIOUS },
    { "Increase Reflexes +1", FALSE, HEALTH, SINGLE, ATT_REA, SUSTAINED, 1, SERIOUS },
    { "Increase Reflexes +2", FALSE, HEALTH, SINGLE, ATT_REA, SUSTAINED, 1, DEADLY },
    { "Increase Reflexes +3", FALSE, HEALTH, SINGLE, ATT_REA, SUSTAINED, 3, DEADLY },
    { "Prophylaxis", FALSE, HEALTH, SINGLE, -1, SUSTAINED, 1, LIGHT },
    { "Resist Pain", FALSE, HEALTH, SINGLE, -1, SUSTAINED, -2, 0 },
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
    { "Acid Stream", TRUE, MANIPULATION, SINGLE, -1, INSTANT, 1, -2 },
    { "Toxic Wave", TRUE, MANIPULATION, AREA, -1, INSTANT, 1, -1 },
    { "Flamethrower", TRUE, MANIPULATION, SINGLE, -1, INSTANT, 1, -2 },
    { "Fireball", TRUE, MANIPULATION, AREA, -1, INSTANT, 1, -1 },
    { "Lightning Bolt", TRUE, MANIPULATION, SINGLE, -1, INSTANT, 1, -2 },
    { "Ball Lightning", TRUE, MANIPULATION, AREA, -1, INSTANT, 1, -1 },
    { "Clout", TRUE, MANIPULATION, SINGLE, -1, INSTANT, 0, -3 },
    { "Poltergeist", TRUE, MANIPULATION, SINGLE, -1, SUSTAINED, 1, MODERATE },
    { "Armour", TRUE, MANIPULATION, SINGLE, -1, SUSTAINED, 2, MODERATE },
    { "Physical Barrier", TRUE, MANIPULATION, SINGLE, -1, SUSTAINED, 2, SERIOUS },
    { "Astral Barrier", FALSE, MANIPULATION, SINGLE, -1, SUSTAINED, 1, SERIOUS },
    { "Ice Sheet", TRUE, MANIPULATION, SINGLE, -1, INSTANT, 1, SERIOUS },
    { "Ignite", TRUE, MANIPULATION, SINGLE, -1, PERMANENT, 1, DEADLY },
    { "Light", TRUE, MANIPULATION, SINGLE, -1, SUSTAINED, 2, MODERATE },
    { "Shadow", TRUE, MANIPULATION, SINGLE, -1, SUSTAINED, 2, MODERATE }
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
    "Whale"
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
    "weapon"
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

const char *eyemods[] = {
  "Camera",
  "Cyber Replacement",
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
  "ASIST Interfance",
  "Chipjack Expert Driver",
  "Cranial Cyberdeck",
  "Data Compactor",
  "Encephalon",
  "Maths SPU",
  "Auto Injector",
  "Balance Tail",
  "Body Compartment",
  "CyberFin",
  "CyberSkull",
  "CyberTorso",
  "Dermal Sheathing",
  "Foot Anchor",
  "Hydraulic Jack",
  "Move By Wire",
  "Climbing Claws",
  "Smartlink",
  "Muscle Replacement"
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
  "Nictating Gland",
  "Orthoskin",
  "Pathogenic Defense",
  "Platelet Factory",
  "Suprathyroid Gland",
  "Symbiotes",
  "Synthacardium",
  "Tailored Pheremones",
  "Toxin Extractor",
  "Tracheal Filter",
  "Cerebral Booster",
  "Damage Compensator",
  "Pain Editor",
  "Reflex Recorder",
  "Synaptic Accelerator",
  "Thermosense Organs",
  "Trauma Dampener"
};

const char *metamagic[] = {
  "Anchoring",
  "Centering",
  "Cleansing",
  "Invoking",
  "Masking",
  "Possessing",
  "Quickening",
  "Reflecting",
  "Shielding"
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
  { "L", "Military Armour" },
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
  "death",                   // 15
  "violence"
};

const char *fire_mode[] =
{
  "NOTHING",
  "Single Shot",
  "Semi-Automatic",
  "Burst Fire",
  "Full Automatic"
};

const char *combat_modifiers[] =
{
  "Recoil",
  "Movement",
  "2Weap",
  "Smart",
  "Distance",
  "Visibility",
  "Position",
  "Gyro",
  "Reach",
  "VehDamaged"
};
