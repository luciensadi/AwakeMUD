#include "structs.hpp"
#include "utils.hpp"
#include "lifestyles.hpp"
#include "house.hpp"
#include "db.hpp"
#include "interpreter.hpp"
#include "comm.hpp"

extern struct landlord *landlords;

struct lifestyle_data lifestyles[NUM_LIFESTYLES] =
{ // Name      Cost/mo
  {"Streets" , 0,
     // The strings the player can select from to represent this lifestyle. First is he/she, second is they.
    {{"Judging by the smell, you're pretty sure $e's homeless.", "Judging by the smell, you're pretty sure they're homeless."},
     {"$s clothes are ragged and hole-filled.", "Their clothes are ragged and hole-filled."},
     {"$e doesn't seem well-acquainted with hygiene.", "They don't seem well-acquainted with hygiene."},
     {"", ""},
     {"", ""},
     {"", ""}}
  },
  {"Squatter", 100,
    {{"You figure $e might have a roof over $s head, but not much of one.", "You figure $e might have a roof over their head, but not much of one."},
     {"The scent of burnt fry oil hangs about $m.", "The scent of burnt fry oil hangs about them."}, // Squatting behind a fast-food place.
     {"$s wrinkled clothes suggest $e lives out of $s car.", "Their wrinkled clothes suggest they live out of their car."},
     {"$e isn't emaciated, but still looks like $e's had to skip a meal or three.", "They aren't emaciated, but still look like they've had to skip a meal or three."},
     {"The distinct scent of public transportation clings to $m.", "The distinct scent of public transportation clings to them."},
     {"$e's got a smell about him-- eau de Coffin Motel?", "They've got a smell about them-- eau de Coffin Motel?"},
  },
  {"Low"     , 1000,
    {{"$e looks like he's fallen on hard times.", "They look like they've fallen on hard times."},
     {"You get the feeling that $e leads a low-class lifestyle.", "You get the feeling that they lead a low-class lifestyle."},
     {"$e probably has a few nuyen to rub together-- but only a few.", "They probably have a few nuyen to rub together-- but only a few."},
     {"$s clothing looks like a cheap knock-off, but it mostly fits and isn't falling apart.", "Their clothing looks like a cheap knock-off, but it mostly fits and isn't falling apart."},
     {"$e doesn't look like he's had to miss any meals recently.", "They don't look like they've had to miss any meals recently."},
     {"", ""}}
  },
  {"Middle"  , 5000,
    {{"$e seems like $e's been able to take care of $mself.", "They seem like they've been able to take care of themselves."},
     {"$e looks like $e probably leads a middle-class life.", "They look like they probably lead a middle-class life."},
     {"$e's not doing badly at all for a shadowrunner.", "They're not doing badly at all for a shadowrunner."},
     {"$e probably showers regularly, as $e smells lightly of cheap soap.", "They probably shower regularly, as they smell lightly of cheap soap."},
     {"$e smells like soap, shampoo, and some sort of cologne or perfume.", "They smell like soap, shampoo, and some sort of cologne or perfume."},
     {"", ""}}
     Her outfit is clean and tailored to fit her, and even features a prominent brand.

She smells like soap, shampoo and some sort of cologne or perfume. Her hygiene is impeccable.

She looks like she's never missed a meal in her life, and has the healthy glow of someone that eats real food.

  },
  {"High"    , 10000,
    {{"$e seems pretty well-off.", "They seem pretty well-off."},
     {"$e looks like he's living the high life.", "They look like they're living the high life."},
     {"$e's doing well for himself.", "They're doing well for themselves."},
     {"", ""},
     {"", ""},
     {"", ""}}
  },
  {"Luxury"  , 100000,
    {{"You feel like $e probably rubs shoulders with the social elite.", "You feel like they probably rub shoulders with the social elite."},
     {"Sharply groomed and tailored, $e knows how take care of $mself.", "Sharply groomed and tailored, they know how take care of themselves."},
     {"There's a sense of privileged wealth about $m.", "There's a sense of privileged wealth about them."},
     {"", ""},
     {"", ""},
     {"", ""}}
  },
};

const char *lifestyle_garage_strings[NUM_GARAGE_STRINGS] = {
  "The stink of old motor oil hangs about $m.",
  "Bits of automotive rust speckle $s outfit.",
  "You catch a whiff of stale exhaust from $m."
};

int house_to_lifestyle_map[NUM_LIFESTYLES] = {
  /* 0 is house's Low    */ LIFESTYLE_LOW,
  /* 1 is house's Middle */ LIFESTYLE_MIDDLE,
  /* 2 is house's High   */ LIFESTYLE_HIGH,
  /* 3 is house's Luxury */ LIFESTYLE_LUXURY
};

// Return a string that describes their lifestyle.
const char *get_lifestyle_string(struct char_data *ch) {
  static char compiled_string[500];

  if (!ch || IS_NPC(ch)) {
    mudlog("SYSERR: Received invalid char to get_lifestyle_string().", ch, LOG_SYSLOG, TRUE);
    return "";
  }

  if (GET_LIFESTYLE_SELECTION(ch) < 0 || GET_LIFESTYLE_SELECTION(ch) >= NUM_LIFESTYLES) {
    mudlog_vfprintf(ch, LOG_SYSLOG, "SYSERR: Lifestyle %d is out of range in get_lifestyle_string().", GET_LIFESTYLE_SELECTION(ch));
    return "";
  }

  if (GET_LIFESTYLE_STRING_SELECTION(ch) < 0 || GET_LIFESTYLE_STRING_SELECTION(ch) >= NUM_STRINGS_PER_LIFESTYLE) {
    mudlog_vfprintf(ch, LOG_SYSLOG, "SYSERR: Lifestyle string selection %d is out of range in get_lifestyle_string().", GET_LIFESTYLE_STRING_SELECTION(ch));
    return "";
  }

  strlcpy(compiled_string, lifestyles[GET_LIFESTYLE_SELECTION(ch)].strings[GET_LIFESTYLE_STRING_SELECTION(ch)][GET_SEX(ch) == SEX_NEUTRAL ? 1 : 0], sizeof(compiled_string));

  if (GET_LIFESTYLE_IS_GARAGE_SELECTION(ch)) {
    snprintf(ENDOF(compiled_string),
                   sizeof(compiled_string) - strlen(compiled_string),
                   " %s",
                   lifestyle_garage_strings[GET_LIFESTYLE_GARAGE_STRING_SELECTION(ch)]);
  }

  return compiled_string;
}

// Iterate through all houses and assign the best owned one to the character.
void determine_lifestyle(struct char_data *ch) {
  int best_lifestyle_found = -1;
  bool lifestyle_is_garage = FALSE;
  vnum_t best_room = -1;

  if (!ch || IS_NPC(ch)) {
    mudlog("SYSERR: Received invalid char to determine_lifestyle()!", ch, LOG_SYSLOG, TRUE);
    return;
  }

  for (struct landlord *llord = landlords; llord; llord = llord->next) {
    for (struct house_control_rec *house = llord->rooms; house; house = house->next) {
      if (house->owner && house->owner == GET_IDNUM(ch)) {
        int found_lifestyle = house_to_lifestyle_map[house->mode];
        if (found_lifestyle >= best_lifestyle_found) {
          struct room_data *room = &world[real_room(house->vnum)];

          if (best_lifestyle_found > found_lifestyle) {
            // If this is a step up, we force-set the garage flag.
            lifestyle_is_garage = ROOM_FLAGGED(room, ROOM_GARAGE);
          } else if (!ROOM_FLAGGED(room, ROOM_GARAGE)) {
            // Otherwise, we remove the is-garage flag if this room isn't a garage.
            lifestyle_is_garage = FALSE;
          }

          // Finally, assign the best lifestyle.
          best_lifestyle_found = found_lifestyle;
          best_room = house->vnum;
        }
      }
    }
  }

  if (best_lifestyle_found == -1) {
    best_lifestyle_found = LIFESTYLE_SQUATTER;
    lifestyle_is_garage = FALSE;
  }

  GET_SETTABLE_LIFESTYLE(ch) = GET_SETTABLE_ORIGINAL_LIFESTYLE(ch) = best_lifestyle_found;
  GET_SETTABLE_LIFESTYLE_IS_GARAGE(ch) = GET_SETTABLE_ORIGINAL_LIFESTYLE_IS_GARAGE(ch) = lifestyle_is_garage;

  log_vfprintf("Assigned %s lifestyle: %d (%s), room %ld is%s garage.",
               GET_CHAR_NAME(ch),
               GET_LIFESTYLE_SELECTION(ch),
               lifestyles[GET_LIFESTYLE_SELECTION(ch)].name,
               best_room,
               GET_LIFESTYLE_IS_GARAGE_SELECTION(ch) ? "" : " NOT");
}

// TODO: Add this to point_update(?) and fill out.
void lifestyle_tick(struct char_data *ch) {}

// TODO: Add lifestyle string selection to customize physical.
// TODO: Rework so that lifestyle string selection is per tier (ex: squatter=1, low=3, etc)

// TODO: Replace this with CUSTOMIZE LIFESTYLE.
ACMD(do_lifestyle) {
  FAILURE_CASE(IS_NPC(ch), "Only player characters can configure their lifestyle.\r\n");

  // No argument? Display current.
  if (!*argument) {
    if (GET_ORIGINAL_LIFESTYLE(ch) != GET_LIFESTYLE_SELECTION(ch)) {
      send_to_char(ch, "You currently present as leading a %s-class lifestyle, but your standard is %d-class.\r\n", GET_LIFESTYLE_SELECTION(ch), GET_ORIGINAL_LIFESTYLE(ch));
    } else {
      send_to_char(ch, "You lead a %s-class lifestyle.\r\n", GET_LIFESTYLE_SELECTION(ch));
    }
    send_to_char(ch, "Your lifestyle is represented as '%s'. Change this with ^WCUSTOMIZE LIFESTYLE^n.", get_lifestyle_string(ch));
    return;
  }
}
