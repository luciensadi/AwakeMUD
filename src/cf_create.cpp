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
#define FORM d->edit_obj

#define CFEDIT_MENU 0
#define CFEDIT_TYPE 1
#define CFEDIT_NAME 2
#define CFEDIT_RATING 3
#define CFEDIT_WOUND 4

extern int get_program_skill(char_data *ch, obj_data *prog, int target);

void cfedit_disp_menu(struct descriptor_data *d)
{
  CLS(CH);
  send_to_char(CH, "1) Name: ^c%s^n\r\n", FORM->restring);
  send_to_char(CH, "2) Type: ^c%s^n\r\n", programs[GET_DESIGN_PROGRAM(FORM)].name);
  send_to_char(CH, "3) Rating: ^c%d^n\r\n", GET_DESIGN_RATING(FORM));

  // Minimum program size is rating^2.
  int program_size = GET_DESIGN_RATING(FORM) * GET_DESIGN_RATING(FORM);
  if (GET_DESIGN_PROGRAM(FORM) == SOFT_ATTACK) {
    // Attack programs multiply size by a factor determined by wound level.
    program_size *= attack_multiplier[GET_DESIGN_PROGRAM_WOUND_LEVEL(FORM)];
    send_to_char(CH, "4) Damage: ^c%s^n\r\n", GET_WOUND_NAME(GET_DESIGN_PROGRAM_WOUND_LEVEL(FORM)));
  } else {
    // Others multiply by a set multiplier based on software type.
    program_size *= programs[GET_DESIGN_PROGRAM(FORM)].multiplier;
  }
  send_to_char(CH, "Effective Size: ^c%d^n\r\n\r\n", program_size);
  send_to_char(CH, "q) Quit and save\r\nEnter your choice: ");
  d->edit_mode = CFEDIT_MENU;
}

void cfedit_disp_program_menu(struct descriptor_data *d)
{
  CLS(CH);

  strncpy(buf, "", sizeof(buf) - 1);

  bool screenreader_mode = PRF_FLAGGED(d->character, PRF_SCREENREADER);
  for (int counter = 1; counter < NUM_PROGRAMS; counter++)
  {
    if (screenreader_mode)
      send_to_char(d->character, "%d) %s\r\n", counter, programs[counter].name);
    else {
      snprintf(ENDOF(buf), sizeof(buf) - strlen(buf), "%s%2d) %-22s%s",
              counter % 3 == 1 ? "  " : "",
              counter,
              programs[counter].name,
              counter % 3 == 0 ? "\r\n" : "");
    }
  }
  if (!screenreader_mode)
    send_to_char(d->character, "%s\r\nSelect program type: ", buf);
  d->edit_mode = CFEDIT_TYPE;
}

void cfedit_parse(struct descriptor_data *d, const char *arg)
{
  int number = atoi(arg);
  switch(d->edit_mode)
  {
  case CFEDIT_MENU:
    switch (*arg) {
    case '1':
      send_to_char(CH, "What do you want to call this complex form: ");
      d->edit_mode = CFEDIT_NAME;
      break;
    case '2':
      cfedit_disp_program_menu(d);
      break;
    case '3':
      if (!GET_DESIGN_PROGRAM(d->edit_obj))
        send_to_char("Choose a program type first!\r\n", CH);
      else {
        send_to_char("Enter Rating: ", CH);
        d->edit_mode = CFEDIT_RATING;
      }
      break;
    case '4':
      if (GET_DESIGN_PROGRAM(d->edit_obj) != SOFT_ATTACK)
        send_to_char(CH, "Invalid option!\r\n");
      else {
        CLS(CH);
        send_to_char(CH, "1) Light\r\n2) Moderate\r\n3) Serious\r\n4) Deadly\r\nEnter your choice: ");
        d->edit_mode = CFEDIT_WOUND;
      }
      break;
    case 'q':
    case 'Q':
      if (!GET_DESIGN_PROGRAM(d->edit_obj) || !GET_DESIGN_RATING(d->edit_obj)) {
        send_to_char("You must specify a program type and/or rating first.\r\n", CH);
        cfedit_disp_menu(d);
        return;
      }

      send_to_char(CH, "Design saved!\r\n");
      if (GET_DESIGN_PROGRAM(d->edit_obj) == SOFT_ATTACK) {
        GET_DESIGN_DESIGNING_TICKS_LEFT(d->edit_obj) = GET_DESIGN_RATING(d->edit_obj) * attack_multiplier[GET_DESIGN_PROGRAM_WOUND_LEVEL(d->edit_obj)];
      } else if (GET_DESIGN_PROGRAM(d->edit_obj) == SOFT_RESPONSE) {
        GET_DESIGN_DESIGNING_TICKS_LEFT(d->edit_obj) = GET_DESIGN_RATING(d->edit_obj) * (GET_DESIGN_RATING(d->edit_obj) * GET_DESIGN_RATING(d->edit_obj));
      } else {
        GET_DESIGN_DESIGNING_TICKS_LEFT(d->edit_obj) = GET_DESIGN_RATING(d->edit_obj) * programs[GET_DESIGN_PROGRAM(d->edit_obj)].multiplier;
      }
      GET_DESIGN_SIZE(d->edit_obj) = GET_DESIGN_RATING(d->edit_obj) * GET_DESIGN_DESIGNING_TICKS_LEFT(d->edit_obj);
      GET_DESIGN_DESIGNING_TICKS_LEFT(d->edit_obj)  *= 20;
      GET_DESIGN_ORIGINAL_TICKS_LEFT(d->edit_obj) = GET_DESIGN_DESIGNING_TICKS_LEFT(d->edit_obj);
      GET_DESIGN_CREATOR_IDNUM(d->edit_obj) = GET_IDNUM(CH);
      obj_to_char(d->edit_obj, CH);
      STATE(d) = CON_PLAYING;
      d->edit_obj = NULL;
      break;
    default:
      send_to_char(CH, "Invalid option!\r\n");
      break;
    }
    break;
  case CFEDIT_RATING:
    GET_DESIGN_RATING(d->edit_obj) = number;
    cfedit_disp_menu(d);
    break;
  case CFEDIT_NAME:
  {
    int length_with_no_color = get_string_length_after_color_code_removal(arg, CH);

    // Silent failure: We already sent the error message in get_string_length_after_color_code_removal().
    if (length_with_no_color == -1) {
      cfedit_disp_menu(d);
      return;
    }
    if (length_with_no_color >= LINE_LENGTH) {
      send_to_char(CH, "That name is too long, please shorten it. The maximum length after color code removal is %d characters.\r\n", LINE_LENGTH - 1);
      cfedit_disp_menu(d);
      return;
    }

    if (strlen(arg) >= MAX_RESTRING_LENGTH) {
      send_to_char(CH, "That restring is too long, please shorten it. The maximum length with color codes included is %d characters.\r\n", MAX_RESTRING_LENGTH - 1);
      cfedit_disp_menu(d);
      return;
    }

    DELETE_ARRAY_IF_EXTANT(d->edit_obj->restring);
    d->edit_obj->restring = str_dup(arg);
    cfedit_disp_menu(d);
    break;
  }
  case CFEDIT_WOUND:
    if (number < LIGHT || number > DEADLY)
      send_to_char(CH, "Not a valid option!\r\nEnter your choice: ");
    else {
      GET_DESIGN_PROGRAM_WOUND_LEVEL(d->edit_obj) = number;
      cfedit_disp_menu(d);
    }
    break;
  case CFEDIT_TYPE:
    if (number < 1 || number >= NUM_PROGRAMS)
      send_to_char(CH, "Not a valid option!\r\nEnter your choice: ");
    else {
      GET_DESIGN_PROGRAM(d->edit_obj) = number;
      GET_DESIGN_RATING(d->edit_obj) = 1;

      if (GET_DESIGN_PROGRAM(d->edit_obj) == SOFT_ATTACK) {
        // Default to Deadly damage.
        GET_DESIGN_PROGRAM_WOUND_LEVEL(d->edit_obj) = DEADLY;
      }

      cfedit_disp_menu(d);
    }
    break;
  }
}

void create_complex_form(struct char_data *ch)
{
  struct obj_data *design = read_object(OBJ_BLANK_COMPLEX_FORM, VIRTUAL, OBJ_LOAD_REASON_CREATE_COMPLEX_FORM);
  STATE(ch->desc) = CON_CF_CREATE;
  design->restring = str_dup("a blank complex form");
  ch->desc->edit_obj = design;
  cfedit_disp_menu(ch->desc);
}

ACMD(do_meditate)
{
  struct obj_data *comp, *prog;
  if (!*argument) {
    if (AFF_FLAGGED(ch, AFF_COMPLEX_FORM_PROGRAM)) {
      AFF_FLAGS(ch).RemoveBit(AFF_COMPLEX_FORM_PROGRAM);
      send_to_char(ch, "You stop working on %s.\r\n", ch->char_specials.programming->restring);
      ch->char_specials.programming = NULL;
    } else
      send_to_char(ch, "Meditate On What?\r\n");
    return;
  }

  if (GET_POS(ch) > POS_SITTING) {
    GET_POS(ch) = POS_SITTING;
    send_to_char(ch, "You find a place to sit and focus.\r\n");
  }

  if (IS_WORKING(ch)) {
    send_to_char(TOOBUSY, ch);
    return;
  }
  else if (ch->in_veh && (ch->vfront || !ch->in_veh->flags.IsSet(VFLAG_WORKSHOP))) {
    send_to_char("You can't do that in here.\r\n", ch);
  }

  if (!GET_DESIGN_PROGRAMMING_TICKS_LEFT(prog)) {
    if (access_level(ch, LVL_ADMIN)) {
      send_to_char(ch, "You use your admin powers to greatly accelerate the development time for %s.\r\n", prog->restring);
      GET_DESIGN_PROGRAMMING_TICKS_LEFT(prog) = 1;
      GET_OBJ_TIMER(prog) = GET_DESIGN_PROGRAMMING_TICKS_LEFT(prog);
    } else {
      send_to_char(ch, "You begin to meditate on %s.\r\n", prog->restring);
      int target = GET_DESIGN_RATING(prog);
      int skill = get_skill(ch, SKILL_COMPUTER, target);
      int success = success_test(skill, target);
      
      if (success > 0) {
        GET_DESIGN_PROGRAMMING_TICKS_LEFT(prog) = 60 * (GET_DESIGN_SIZE(prog) / success);
        GET_DESIGN_ORIGINAL_TICKS_LEFT(prog) = GET_DESIGN_PROGRAMMING_TICKS_LEFT(prog);
      } else {
        GET_DESIGN_PROGRAMMING_TICKS_LEFT(prog) = number(1, 6) + number(1, 6);
        GET_DESIGN_ORIGINAL_TICKS_LEFT(prog) = (GET_DESIGN_SIZE(prog) * 60) / number(1, 3);
        GET_DESIGN_PROGRAMMING_FAILED(prog) = 1;
      }
    }
  } else
    send_to_char(ch, "You continue to work on %s.\r\n", prog->restring);
  AFF_FLAGS(ch).SetBit(AFF_COMPLEX_FORM_PROGRAM);
  GET_BUILDING(ch) = prog;
}