#include "structs.h"
#include "awake.h"
#include "db.h"
#include "comm.h"
#include "interpreter.h"
#include "handler.h"
#include "utils.h"
#include "screen.h"
#include "constants.h"
#include "olc.h"
#include "newmagic.h"

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
  send_to_char(CH, "3) Category: ^c%s^n\r\n", spell_category[GET_OBJ_VAL(SPELL, 9)]);
  send_to_char(CH, "4) Type: ^c%s^n\r\n", spells[GET_OBJ_VAL(SPELL, 1)].name);
  send_to_char(CH, "5) Force: ^c%d^n\r\n", GET_OBJ_VAL(SPELL, 0));
  if (GET_OBJ_VAL(SPELL, 1) == SPELL_INCATTR || GET_OBJ_VAL(SPELL, 1) == SPELL_INCCYATTR ||
      GET_OBJ_VAL(SPELL, 1) == SPELL_DECATTR || GET_OBJ_VAL(SPELL, 1) == SPELL_DECCYATTR)
    send_to_char(CH, "6) Attribute: ^c%s^n\r\n", attributes[GET_OBJ_VAL(SPELL, 3)]);
  send_to_char(CH, "q) Quit\r\nEnter your choice: ");
  d->edit_mode = SPEDIT_MENU;
}

void spedit_disp_spell_type(struct descriptor_data *d)
{
  int x = 1;
  for (int i = 0; i < MAX_SPELLS; i++) 
    if (spells[i].category == GET_OBJ_VAL(SPELL, 9)) {
      send_to_char(CH, "%d) %s\r\n", x, spells[i].name);
      x++;
    }
  send_to_char("Enter spell type: ", CH);
  d->edit_mode = SPEDIT_TYPE;
}

void create_spell(struct char_data *ch)
{
  struct obj_data *design = read_object(117, VIRTUAL);
  STATE(ch->desc) = CON_SPELL_CREATE;
  GET_OBJ_TIMER(design) = -2;
  GET_OBJ_VAL(design, 0) = 1;
  GET_OBJ_VAL(design, 2) = GET_TRADITION(ch) == TRAD_SHAMANIC ? 1 : 0;
  GET_OBJ_VAL(design, 8) = GET_IDNUM(ch);
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
      DELETE_ARRAY_IF_EXTANT(d->str);
      d->str = new (char *);
      *(d->str) = NULL;
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
      if (!GET_OBJ_VAL(SPELL, 9))
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
      if (GET_OBJ_VAL(SPELL, 1) == SPELL_INCATTR || GET_OBJ_VAL(SPELL, 1) == SPELL_INCCYATTR ||
          GET_OBJ_VAL(SPELL, 1) == SPELL_DECATTR || GET_OBJ_VAL(SPELL, 1) == SPELL_DECCYATTR) {
        CLS(CH);
        for (int i = 0; i < 6; i++)
          send_to_char(CH, "%d) %s\r\n", i, attributes[i]);
        send_to_char("Select attribute: ", CH);
        d->edit_mode = SPEDIT_ATTR;
        break;
       }
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
      GET_OBJ_VAL(SPELL, 3) = number;
      spedit_disp_menu(d);
      break;
    case SPEDIT_NAME:
      if (strlen(arg) >= LINE_LENGTH) {
        spedit_disp_menu(d);
        return;
      }
      DELETE_ARRAY_IF_EXTANT(SPELL->restring);
      SPELL->restring = str_dup(arg);
      spedit_disp_menu(d);
      break;
    case SPEDIT_FORCE: 
      x = MIN(d->edit_number2, GET_SKILL(CH, SKILL_SPELLDESIGN) ? GET_SKILL(CH, SKILL_SPELLDESIGN) : GET_SKILL(CH, SKILL_SORCERY));
      if (number > x || number < 1)
        send_to_char(CH, "Force must be between 1 and %d. Enter Force: ", x);
      else {
        GET_OBJ_VAL(SPELL, 0) = number;
        spedit_disp_menu(d);
      }
      break;
    case SPEDIT_CATEGORY: 
      if (number < 1 || number > 5)
        send_to_char("Please select from the list. Enter Category: ", CH);
      else {
        GET_OBJ_VAL(SPELL, 9) = number;
        spedit_disp_menu(d);
      }
      break;
    case SPEDIT_TYPE:
      for (i = 0; i < MAX_SPELLS; i++)
        if (spells[i].category == GET_OBJ_VAL(SPELL, 9) && x++ == number)
          break;
      if (i == MAX_SPELLS)
        send_to_char("Invalid spell type. Enter spell type: ", CH);
      else {
        GET_OBJ_VAL(SPELL, 1) = i;
        GET_OBJ_VAL(SPELL, 3) = 0;
        spedit_disp_menu(d);
      }
      break;
  }
}

void spell_design(struct char_data *ch, struct obj_data *formula)
{
  if (GET_OBJ_VAL(formula, 8) != GET_IDNUM(ch)) {
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
  struct obj_data *lib = world[ch->in_room].contents;
  for (; lib; lib = lib->next_content)
    if (GET_OBJ_TYPE(lib) == ITEM_MAGIC_TOOL && GET_OBJ_VAL(lib, 1) >= GET_OBJ_VAL(formula, 0) &&
        ((GET_TRADITION(ch) == TRAD_SHAMANIC
          && GET_OBJ_VAL(lib, 0) == TYPE_LODGE && GET_OBJ_VAL(lib, 3) == GET_IDNUM(ch)) ||
         (GET_TRADITION(ch) != TRAD_SHAMANIC && GET_OBJ_VAL(lib, 0) == TYPE_LIBRARY_SPELL)))
      break;
  if (!lib) {
    send_to_char("You don't have the right tools here to design that spell.\r\n", ch);
    return;
  }
  if (GET_TRADITION(ch) == TRAD_SHAMANIC && GET_OBJ_VAL(lib, 9)) {
    send_to_char("You need to finish building that lodge before you can use it.\r\n", ch);
    return;
  }
  if (GET_OBJ_VAL(formula, 6))
    send_to_char("You continue to design that spell.\r\n", ch);
  else if (!GET_OBJ_TIMER(formula))
    send_to_char("That spell is already complete.\r\n", ch);
  else {
    int skill = GET_SKILL(ch, SKILL_SPELLDESIGN), target = 0, x = 0;
    if (!skill || skill < GET_OBJ_VAL(formula, 0)) {
      skill = GET_SKILL(ch, SKILL_SORCERY);
      target = 2;
    }
    if (skill < GET_OBJ_VAL(formula, 0)) {
      send_to_char("You don't have the neccesary skills to design this spell.\r\n", ch);
      return;
    }
    int drain = spells[GET_OBJ_VAL(formula, 1)].draindamage;
    if (drain < 1 || drain == DEADLY)
      drain = 60;
    else if (drain == SERIOUS)
      drain = 36;
    else if (drain == MODERATE)
      drain = 18;
    else drain = 6;
    drain *= 1440;
    target += (GET_OBJ_VAL(formula, 0) * 2) - (int)(GET_REAL_MAG(ch) / 100);
    if (spells[GET_OBJ_VAL(formula, 1)].category == DETECTION || spells[GET_OBJ_VAL(formula, 1)].category == ILLUSION)
      target--;
    if (spells[GET_OBJ_VAL(formula, 1)].physical)
      target++;
    if (spells[GET_OBJ_VAL(formula, 1)].duration == SUSTAINED)
      target++;
    if (GET_OBJ_VAL(formula, 1) == SPELL_DETOX || GET_OBJ_VAL(formula, 1) == SPELL_RESISTPAIN)
      target -=2;
    if (GET_OBJ_VAL(formula, 1) == SPELL_CLOUT || GET_OBJ_VAL(formula, 1) == SPELL_STUNBALL ||
        GET_OBJ_VAL(formula, 1) == SPELL_STUNBOLT)
      target--;
    if (GET_TRADITION(ch) == TRAD_SHAMANIC)
      totem_bonus(ch, 0, GET_OBJ_VAL(formula, 1), x, skill);
    else if (GET_TRADITION(ch) == TRAD_HERMETIC && GET_SPIRIT(ch)) {
      int startskill = skill;
      for (struct spirit_data *spir = GET_SPIRIT(ch); spir && skill == startskill; spir = spir->next)
        if (spir->called) {
          struct char_data *spirit = find_spirit_by_id(spir->id, GET_IDNUM(ch));
          if (MOB_FLAGS(spirit).IsSet(MOB_STUDY)) {
            switch(spir->type) {
            case ELEM_FIRE:
              if (spells[GET_OBJ_VAL(formula, 1)].category == COMBAT) {
                skill += spir->force;
                MOB_FLAGS(spirit).RemoveBit(MOB_STUDY);
              }
              break;
            case ELEM_WATER:
              if (spells[GET_OBJ_VAL(formula, 1)].category == ILLUSION) {
                skill += spir->force;
                MOB_FLAGS(spirit).RemoveBit(MOB_STUDY);
              }
              break;
            case ELEM_AIR:
              if (spells[GET_OBJ_VAL(formula, 1)].category == DETECTION) {
                skill += spir->force;
                MOB_FLAGS(spirit).RemoveBit(MOB_STUDY);
              }
              break;
            case ELEM_EARTH:
              if (spells[GET_OBJ_VAL(formula, 1)].category == MANIPULATION) {
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
    if (GET_OBJ_VAL(lib, 1) > skill)
      skill += (GET_OBJ_VAL(lib, 1) - skill) / 2;
    int success = success_test(skill, target);
    if (success < 1) {
      GET_OBJ_VAL(formula, 6) = drain / 2;
      GET_OBJ_VAL(formula, 7) = drain;
      GET_OBJ_TIMER(formula) = -3;
    } else
      GET_OBJ_VAL(formula, 6) = GET_OBJ_VAL(formula, 7) = drain / success;
    send_to_char(ch, "You start designing %s.\r\n", GET_OBJ_NAME(formula));
  }
  act("$n sits down and begins to design a spell.", TRUE, ch, 0, 0, TO_ROOM);
  GET_BUILDING(ch) = formula;
  AFF_FLAGS(ch).SetBit(AFF_SPELLDESIGN);
}
