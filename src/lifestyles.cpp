#include "structs.hpp"
#include "utils.hpp"
#include "lifestyles.hpp"
#include "house.hpp"
#include "db.hpp"

extern struct landlord *landlords;

struct lifestyle_data lifestyles[NUM_LIFESTYLES] =
{ // Name      Cost/mo
  {"Streets" , 0,
     // The strings the player can select from to represent this lifestyle. First is he/she, second is they.
    {{"Judging by the smell, you're pretty sure $e's homeless.", "Judging by the smell, you're pretty sure they're homeless."},
     {"$s clothes are ragged and hole-filled.", "Their clothes are ragged and hole-filled."},
     {"$e doesn't seem well-acquainted with hygiene.", "They don't seem well-acquainted with hygiene."}}
  },
  {"Squatter", 100,
    {{"You figure $e probably has a roof over $s head, but not much of one.", "You figure they probably have a roof over their head, but not much of one."},
     {"The scent of burnt fry oil hangs about $m.", "The scent of burnt fry oil hangs about them."}, // Squatting behind a fast-food place.
     {"$s wrinkled clothes suggest $e lives out of $s car.", "Their wrinkled clothes suggest they live out of their car."}}
  },
  {"Low"     , 1000,
    {{"$e looks like he's fallen on hard times.", "They look like they've fallen on hard times."},
     {"You get the feeling that $e leads a low-class lifestyle.", "You get the feeling that they lead a low-class lifestyle."},
     {"$e probably has a few nuyen to rub together-- but only a few.", "They probably have a few nuyen to rub together-- but only a few."}}
  },
  {"Middle"  , 5000,
    {{"$e seems like $e's been able to take care of $mself.", "They seem like they've been able to take care of themselves."},
     {"$e looks like $e probably leads a middle-class life.", "They look like they probably lead a middle-class life."},
     {"$e's not doing badly at all for a shadowrunner.", "They're not doing badly at all for a shadowrunner."}}
  },
  {"High"    , 10000,
    {{"$e seems pretty well-off.", "They seem pretty well-off."},
     {"$e looks like he's living the high life.", "They look like they're living the high life."},
     {"$e's doing well for himself.", "They're doing well for themselves."}}
  },
  {"Luxury"  , 100000,
    {{"You feel like $e probably rubs shoulders with the social elite.", "You feel like they probably rub shoulders with the social elite."},
     {"Sharply groomed and tailored, $e knows how take care of $mself.", "Sharply groomed and tailored, they know how take care of themselves."},
     {"There's a sense of privileged wealth about $m.", "There's a sense of privileged wealth about them."}}
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

  if (GET_LIFESTYLE(ch) < 0 || GET_LIFESTYLE(ch) >= NUM_LIFESTYLES) {
    mudlog_vfprintf(ch, LOG_SYSLOG, "SYSERR: Lifestyle %d is out of range in get_lifestyle_string().", GET_LIFESTYLE(ch));
    return "";
  }

  if (GET_LIFESTYLE_STRING_SELECTION(ch) < 0 || GET_LIFESTYLE_STRING_SELECTION(ch) >= NUM_STRINGS_PER_LIFESTYLE) {
    mudlog_vfprintf(ch, LOG_SYSLOG, "SYSERR: Lifestyle string selection %d is out of range in get_lifestyle_string().", GET_LIFESTYLE_STRING_SELECTION(ch));
    return "";
  }

  strlcpy(compiled_string, lifestyles[GET_LIFESTYLE(ch)].strings[GET_LIFESTYLE_STRING_SELECTION(ch)][GET_SEX(ch) == SEX_NEUTRAL ? 1 : 0], sizeof(compiled_string));

  if (GET_LIFESTYLE_IS_GARAGE(ch)) {
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

  GET_SETTABLE_LIFESTYLE(ch) = best_lifestyle_found;
  GET_SETTABLE_LIFESTYLE_IS_GARAGE(ch) = lifestyle_is_garage;

  mudlog_vfprintf(ch, LOG_MISCLOG, "Assigned %s lifestyle: %d (%s), room %ld is%s garage.",
                  GET_CHAR_NAME(ch),
                  GET_LIFESTYLE(ch),
                  lifestyles[GET_LIFESTYLE(ch)].name,
                  best_room,
                  GET_LIFESTYLE_IS_GARAGE(ch) ? "" : " NOT");
}
