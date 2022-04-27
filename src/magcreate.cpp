#include "structs.hpp"
#include "awake.hpp"
#include "db.hpp"
#include "comm.hpp"
#include "interpreter.hpp"
#include "handler.hpp"
#include "utils.hpp"
#include "screen.hpp"
#include "constants.hpp"
#include "olc.hpp"
#include "newmagic.hpp"

#define CH d->character
#define SPELL d->edit_obj
#define SPEDIT_MENU	1
#define SPEDIT_NAME	2
#define SPEDIT_DESC	3
#define SPEDIT_CATEGORY	4
#define SPEDIT_FORCE	5
#define SPEDIT_TYPE	7
#define SPEDIT_ATTR	8

void spedit_disp_menu(struct descriptor_data *d)
{
  CLS(CH);
  send_to_char(CH, "1) Name: ^c%s^n\r\n", SPELL->restring);
  send_to_char(CH, "2) Look Desc:\r\n^c%s^n\r\n", SPELL->photo);
  send_to_char(CH, "3) Category: ^c%s^n\r\n", spell_category[GET_SPELLFORMULA_CATEGORY(SPELL)]);
  send_to_char(CH, "4) Type: ^c%s%s^n\r\n",
               spells[GET_SPELLFORMULA_SPELL(SPELL)].name,
               spell_is_nerp(GET_SPELLFORMULA_SPELL(SPELL)) ? " ^Y(not implemented)^n" : "");
  send_to_char(CH, "5) Force: ^c%d^n\r\n", GET_SPELLFORMULA_FORCE(SPELL));

  if (GET_SPELLFORMULA_SPELL(SPELL) == SPELL_INCATTR || GET_SPELLFORMULA_SPELL(SPELL) == SPELL_INCCYATTR ||
      GET_SPELLFORMULA_SPELL(SPELL) == SPELL_DECATTR || GET_SPELLFORMULA_SPELL(SPELL) == SPELL_DECCYATTR)
    send_to_char(CH, "6) Attribute: ^c%s^n\r\n", attributes[GET_SPELLFORMULA_SUBTYPE(SPELL)]);

  send_to_char(CH, "q) Quit\r\nEnter your choice: ");
  d->edit_mode = SPEDIT_MENU;
}

void spedit_disp_spell_type(struct descriptor_data *d)
{
  int x = 1;
  for (int i = 0; i < MAX_SPELLS; i++)
    if (spells[i].category == GET_SPELLFORMULA_CATEGORY(SPELL)) {
      send_to_char(CH, "%d) %s%s\r\n", x, spells[i].name, spell_is_nerp(i) ? "  ^y(not implemented)^n" : "");
      x++;
    }
  send_to_char("Enter spell type: ", CH);
  d->edit_mode = SPEDIT_TYPE;
}

void create_spell(struct char_data *ch)
{
  struct obj_data *design = read_object(OBJ_BLANK_SPELL_FORMULA, VIRTUAL);
  STATE(ch->desc) = CON_SPELL_CREATE;
  GET_OBJ_TIMER(design) = -2;
  GET_SPELLFORMULA_FORCE(design) = 1;
  GET_SPELLFORMULA_TRADITION(design) = GET_TRADITION(ch) == TRAD_SHAMANIC ? 1 : 0;
  GET_SPELLFORMULA_CREATOR_IDNUM(design) = GET_IDNUM(ch);
  design->restring = str_dup("A blank spell formula");
  design->photo = str_dup("It looks very blank.");
  ch->desc->edit_obj = design;
  spedit_disp_menu(ch->desc);
}

void spedit_parse(struct descriptor_data *d, const char *arg)
{
  int number = atoi(arg), i = 0, x = 1;
  switch(d->edit_mode)
  {
  case SPEDIT_MENU:
    switch (*arg) {
      case '1':
        send_to_char("Enter spell name: ", CH);
        d->edit_mode = SPEDIT_NAME;
        break;
      case '2':
        send_to_char(CH, "Enter spell design description:\r\n");
        d->edit_mode = SPEDIT_DESC;
        DELETE_D_STR_IF_EXTANT(d);
        INITIALIZE_NEW_D_STR(d);
        d->max_str = MAX_MESSAGE_LENGTH;
        d->mail_to = 0;
        break;
      case '3':
        CLS(CH);
        for (i = 1; i < 6; i++)
          send_to_char(CH, "%d) %s\r\n", i, spell_category[i]);
        send_to_char("Enter spell category: ", CH);
        d->edit_mode = SPEDIT_CATEGORY;
        break;
      case '4':
        if (!GET_SPELLFORMULA_CATEGORY(SPELL))
          send_to_char("You must select a category first. Enter your choice: ", CH);
        else spedit_disp_spell_type(d);
        break;
      case '5':
        send_to_char("Enter spell force: ", CH);
        d->edit_mode = SPEDIT_FORCE;
        break;
      case 'Q':
      case 'q':
        send_to_char(CH, "Design saved!\r\n");
        obj_to_char(d->edit_obj, CH);
        STATE(d) = CON_PLAYING;
        d->edit_obj = NULL;
        break;
      case '6':
        if (GET_SPELLFORMULA_SPELL(SPELL) == SPELL_INCATTR || GET_SPELLFORMULA_SPELL(SPELL) == SPELL_INCCYATTR ||
            GET_SPELLFORMULA_SPELL(SPELL) == SPELL_DECATTR || GET_SPELLFORMULA_SPELL(SPELL) == SPELL_DECCYATTR) {
          CLS(CH);
          for (int i = 0; i < 6; i++)
            send_to_char(CH, "%d) %s\r\n", i, attributes[i]);
          send_to_char("Select attribute: ", CH);
          d->edit_mode = SPEDIT_ATTR;
          break;
         }
        // Explicit fallthrough-- you can only select an attribute for attribute-linked spells.
        // fall through
      default:
        send_to_char(CH, "Invalid option!\r\n");
        break;
    }
    break;
    case SPEDIT_ATTR:
      if (number > 5) {
        send_to_char("Invalid Attributes. Select attribute: ", CH);
        return;
      }
      GET_SPELLFORMULA_SUBTYPE(SPELL) = number;
      spedit_disp_menu(d);
      break;
    case SPEDIT_NAME: {
      int length_with_no_color = get_string_length_after_color_code_removal(arg, CH);

      // Silent failure: We already sent the error message in get_string_length_after_color_code_removal().
      if (length_with_no_color == -1) {
        spedit_disp_menu(d);
        return;
      }
      if (length_with_no_color >= LINE_LENGTH) {
        send_to_char(CH, "That name is too long, please shorten it. The maximum length after color code removal is %d characters.\r\n", LINE_LENGTH - 1);
        spedit_disp_menu(d);
        return;
      }

      if (strlen(arg) >= MAX_RESTRING_LENGTH) {
        send_to_char(CH, "That restring is too long, please shorten it. The maximum length with color codes included is %d characters.\r\n", MAX_RESTRING_LENGTH - 1);
        spedit_disp_menu(d);
        return;
      }

      DELETE_ARRAY_IF_EXTANT(SPELL->restring);
      SPELL->restring = str_dup(arg);
      spedit_disp_menu(d);
      break;
    }
    case SPEDIT_FORCE:
      x = MIN(d->edit_number2, GET_SKILL(CH, SKILL_SPELLDESIGN) ? GET_SKILL(CH, SKILL_SPELLDESIGN) : GET_SKILL(CH, SKILL_SORCERY));
      if (number > x || number < 1)
        send_to_char(CH, "Force must be between 1 and %d. Enter Force: ", x);
      else {
        GET_SPELLFORMULA_FORCE(SPELL) = number;
        spedit_disp_menu(d);
      }
      break;
    case SPEDIT_CATEGORY:
      if (number < 1 || number > 5)
        send_to_char("Please select from the list. Enter Category: ", CH);
      else {
        if (GET_SPELLFORMULA_CATEGORY(SPELL) != number) {
          GET_SPELLFORMULA_SPELL(SPELL) = 0;
          GET_SPELLFORMULA_SUBTYPE(SPELL) = 0;
          GET_SPELLFORMULA_CATEGORY(SPELL) = number;
        }
        spedit_disp_menu(d);
      }
      break;
    case SPEDIT_TYPE:
      for (i = 0; i < MAX_SPELLS; i++)
        if (spells[i].category == GET_SPELLFORMULA_CATEGORY(SPELL) && x++ == number)
          break;
      if (i == MAX_SPELLS)
        send_to_char("Invalid spell type. Enter spell type: ", CH);
      else {
        GET_SPELLFORMULA_SPELL(SPELL) = i;
        GET_SPELLFORMULA_SUBTYPE(SPELL) = 0;
        spedit_disp_menu(d);
      }
      break;
  }
}

void spell_design(struct char_data *ch, struct obj_data *formula)
{
  struct obj_data *lib = NULL;

  if (GET_SPELLFORMULA_CREATOR_IDNUM(formula) != GET_IDNUM(ch)) {
    send_to_char("You don't understand where the creator of this spell is coming from.\r\n", ch);
    return;
  }
  if (IS_WORKING(ch)) {
    send_to_char("You are already working on something.\r\n", ch);
    return;
  }
  if (GET_POS(ch) > POS_SITTING) {
    send_to_char("Take a seat before beginning to design a spell.\r\n", ch);
    return;
  }
  FOR_ITEMS_AROUND_CH(ch, lib) {
    if (GET_OBJ_TYPE(lib) == ITEM_MAGIC_TOOL && GET_MAGIC_TOOL_RATING(lib) >= GET_SPELLFORMULA_FORCE(formula)
        && ((GET_TRADITION(ch) == TRAD_SHAMANIC && GET_MAGIC_TOOL_TYPE(lib) == TYPE_LODGE && GET_MAGIC_TOOL_OWNER(lib) == GET_IDNUM(ch))
            || (GET_TRADITION(ch) != TRAD_SHAMANIC && GET_MAGIC_TOOL_TYPE(lib) == TYPE_LIBRARY_SPELL)))
    {
      break;
    }
  }

  if (!lib) {
    send_to_char("You don't have the right tools here to design that spell.\r\n", ch);
    return;
  }

  if (GET_TRADITION(ch) == TRAD_SHAMANIC && GET_MAGIC_TOOL_BUILD_TIME_LEFT(lib)) {
    send_to_char("You need to finish building that lodge before you can use it.\r\n", ch);
    return;
  }

  if (GET_SPELLFORMULA_TIME_LEFT(formula)) {
    send_to_char("You continue to design that spell.\r\n", ch);
    act("$n sits down and begins to design a spell.", TRUE, ch, 0, 0, TO_ROOM);
    GET_BUILDING(ch) = formula;
    AFF_FLAGS(ch).SetBit(AFF_SPELLDESIGN);
    return;
  }

  if (!GET_OBJ_TIMER(formula)) {
    send_to_char("That spell is already complete.\r\n", ch);
    return;
  }

  int skill = GET_SKILL(ch, SKILL_SPELLDESIGN);
  int target = (GET_SPELLFORMULA_FORCE(formula) * 2) - (int)(GET_REAL_MAG(ch) / 100);

  // Default to sorcery, if applicable. This increases the TN by 2.
  if (skill < GET_SPELLFORMULA_FORCE(formula)) {
    if (GET_SKILL(ch, SKILL_SORCERY) >= GET_SPELLFORMULA_FORCE(formula)) {
      send_to_char("Finding your spell design knowledge insufficient for this task, you fall back on your general grounding in sorcery.\r\n", ch);
      skill = GET_SKILL(ch, SKILL_SORCERY);
      target += 2;
    } else {
      send_to_char("You don't have the necessary skills to design this spell. Ideally, you want Spell Design at or above the spell's force, but Sorcery at that force will suffice.\r\n", ch);
      return;
    }
  }

  // MitS p48: if library/lodge exceeds your base skill, skill += (tools - skill) / 2
  if (GET_MAGIC_TOOL_RATING(lib) > skill)
    skill += (GET_MAGIC_TOOL_RATING(lib) - skill) / 2;

  // Design duration is based on drain level and is measured in days, per MitS p48.
  int design_duration = spells[GET_SPELLFORMULA_SPELL(formula)].draindamage;

  // Drain is less than 1 for all variable drain spells. Per MitS p48, we use the highest applicable drain for these, which we assume to be deadly.
  switch (design_duration) {
    case LIGHT:
      design_duration = 6;
      break;
    case MODERATE:
      design_duration = 18;
      break;
    case SERIOUS:
      design_duration = 36;
      break;
    case DEADLY:
      // fall through
    default:
      design_duration = 60;
      break;
  }
  design_duration *= 1440;

  // Ripped out this weird houserule stuff, I can't find it in the books anywhere. -LS
  /*
  if (spells[GET_SPELLFORMULA_SPELL(formula)].category == DETECTION || spells[GET_SPELLFORMULA_SPELL(formula)].category == ILLUSION)
    target--;
  if (spells[GET_SPELLFORMULA_SPELL(formula)].physical)
    target++;
  if (spells[GET_SPELLFORMULA_SPELL(formula)].duration == SUSTAINED)
    target++;
  if (GET_SPELLFORMULA_SPELL(formula) == SPELL_DETOX || GET_SPELLFORMULA_SPELL(formula) == SPELL_RESISTPAIN)
    target -= 2;
  if (GET_SPELLFORMULA_SPELL(formula) == SPELL_CLOUT || GET_SPELLFORMULA_SPELL(formula) == SPELL_STUNBALL ||
      GET_SPELLFORMULA_SPELL(formula) == SPELL_STUNBOLT)
    target--;
  */

  // Apply totem bonuses to their skill. We use the _throwaway_value parameter here because this doesn't affect our TN.
  if (GET_TRADITION(ch) == TRAD_SHAMANIC) {
    int _throwaway_value = 0;
    totem_bonus(ch, SPELLCASTING, GET_SPELLFORMULA_SPELL(formula), _throwaway_value, skill);
  }
  // If they're hermetic, they get to apply elemental study bonuses to their spell creation.
  else if (GET_TRADITION(ch) == TRAD_HERMETIC && GET_SPIRIT(ch)) {
    // Used to flag when we need to stop executing.
    int startskill = skill;
    for (struct spirit_data *spir = GET_SPIRIT(ch); spir && skill == startskill; spir = spir->next) {
      if (spir->called) {
        struct char_data *spirit = find_spirit_by_id(spir->id, GET_IDNUM(ch));
        if (MOB_FLAGS(spirit).IsSet(MOB_STUDY)) {
          switch(spir->type) {
            case ELEM_FIRE:
              if (spells[GET_SPELLFORMULA_SPELL(formula)].category == COMBAT) {
                skill += spir->force;
                MOB_FLAGS(spirit).RemoveBit(MOB_STUDY);
              }
              break;
            case ELEM_WATER:
              if (spells[GET_SPELLFORMULA_SPELL(formula)].category == ILLUSION) {
                skill += spir->force;
                MOB_FLAGS(spirit).RemoveBit(MOB_STUDY);
              }
              break;
            case ELEM_AIR:
              if (spells[GET_SPELLFORMULA_SPELL(formula)].category == DETECTION) {
                skill += spir->force;
                MOB_FLAGS(spirit).RemoveBit(MOB_STUDY);
              }
              break;
            case ELEM_EARTH:
              if (spells[GET_SPELLFORMULA_SPELL(formula)].category == MANIPULATION) {
                skill += spir->force;
                MOB_FLAGS(spirit).RemoveBit(MOB_STUDY);
              }
              break;
          }
          elemental_fulfilled_services(ch, spirit, spir);
          break;
        }
      }
    }
  }


  int success = success_test(skill, target);
  if (success < 1) {
    GET_SPELLFORMULA_TIME_LEFT(formula) = design_duration / 2;
    GET_SPELLFORMULA_INITIAL_TIME(formula) = design_duration;
    GET_OBJ_TIMER(formula) = SPELL_DESIGN_FAILED_CODE;
  } else
    GET_SPELLFORMULA_TIME_LEFT(formula) = GET_SPELLFORMULA_INITIAL_TIME(formula) = design_duration / success;

  if (access_level(ch, LVL_ADMIN)) {
    send_to_char("You use your staff privileges to greatly accelerate the design process.\r\n", ch);
    GET_SPELLFORMULA_TIME_LEFT(formula) = GET_SPELLFORMULA_INITIAL_TIME(formula) = 1;
  }

  send_to_char(ch, "You start designing %s.\r\n", GET_OBJ_NAME(formula));
  act("$n sits down and begins to design a spell.", TRUE, ch, 0, 0, TO_ROOM);
  GET_BUILDING(ch) = formula;
  AFF_FLAGS(ch).SetBit(AFF_SPELLDESIGN);
}
