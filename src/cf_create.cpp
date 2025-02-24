#include "structs.hpp"
#include "awake.hpp"
#include "utils.hpp"
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

#define COMPLEX_FORM_TYPES 17

int complex_form_programs[COMPLEX_FORM_TYPES] = {
  SOFT_ATTACK,
  SOFT_SLOW,
  SOFT_MEDIC,
  SOFT_COMPRESSOR,
  SOFT_DECRYPT,
  SOFT_RELOCATE,
  SOFT_SLEAZE,
  SOFT_TRACK,
  SOFT_ARMOR,
  SOFT_CAMO,
  SOFT_CRASH,
  SOFT_DEFUSE,
  SOFT_EVALUATE,
  SOFT_VALIDATE,
  SOFT_SWERVE,
  SOFT_CLOAK,
  SOFT_LOCKON
};

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
  for (int counter = 0; counter < COMPLEX_FORM_TYPES; counter++)
  {
    program_data* prog_data = &programs[complex_form_programs[counter]];
    if (screenreader_mode)
      send_to_char(d->character, "%d) %s\r\n", counter + 1, prog_data->name);
    else {
      snprintf(ENDOF(buf), sizeof(buf) - strlen(buf), "%s%2d) %-22s%s",
              (counter + 1) % 3 == 1 ? "  " : "",
              counter + 1,
              prog_data->name,
              (counter + 1) % 3 == 0 ? "\r\n" : "");
    }
  }
  if (!screenreader_mode)
    send_to_char(d->character, "%s\r\nSelect program type:\r\n", buf);
  d->edit_mode = CFEDIT_TYPE;
}

void cfedit_parse(struct descriptor_data *d, const char *arg)
{
  int target, success, intelligence;
  struct obj_data *asist;
  struct char_data *ch = CH;
  FAILURE_CASE(!(asist = find_cyberware(ch, CYB_ASIST)), "You don't have an asist converter.");

  int option_n = atoi(arg);
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

      send_to_char(CH, "Complex form saved as %s!\r\n", d->edit_obj->restring);

      target = GET_DESIGN_RATING(d->edit_obj);
      intelligence = GET_REAL_INT(CH);
      success = success_test(intelligence, target);
      if (success <= 0) {
        GET_DESIGN_PROGRAMMING_TICKS_LEFT(d->edit_obj) = number(1, 6) + number(1, 6);
        GET_DESIGN_ORIGINAL_TICKS_LEFT(d->edit_obj) = (GET_DESIGN_SIZE(d->edit_obj) * 60) / number(1, 3);
        GET_DESIGN_PROGRAMMING_FAILED(d->edit_obj) = 1;
      } else {
        if (GET_DESIGN_PROGRAM(d->edit_obj) == SOFT_ATTACK) {
          GET_DESIGN_PROGRAMMING_TICKS_LEFT(d->edit_obj) = GET_DESIGN_RATING(d->edit_obj) * attack_multiplier[GET_DESIGN_PROGRAM_WOUND_LEVEL(d->edit_obj)];
        } else {
          GET_DESIGN_PROGRAMMING_TICKS_LEFT(d->edit_obj) = GET_DESIGN_RATING(d->edit_obj) * programs[GET_DESIGN_PROGRAM(d->edit_obj)].multiplier;
        }
        
        GET_DESIGN_SIZE(d->edit_obj) = GET_DESIGN_RATING(d->edit_obj) * GET_DESIGN_PROGRAMMING_TICKS_LEFT(d->edit_obj);
        GET_DESIGN_PROGRAMMING_TICKS_LEFT(d->edit_obj) *= 20;
        GET_DESIGN_ORIGINAL_TICKS_LEFT(d->edit_obj) = GET_DESIGN_PROGRAMMING_TICKS_LEFT(d->edit_obj);
        GET_DESIGN_CREATOR_IDNUM(d->edit_obj) = GET_IDNUM(CH);
      }

      obj_to_obj(d->edit_obj, asist);
      STATE(d) = CON_PLAYING;
      d->edit_obj = NULL;
      break;
    default:
      send_to_char(CH, "Invalid option!\r\n");
      break;
    }
    break;
  case CFEDIT_RATING:
    if (option_n > GET_SKILL(CH, SKILL_COMPUTER)) {
      send_to_char(CH, "You can't create a program of a higher rating than your computer skill.\r\n"
                   "Enter Rating: ");
    } else {
      GET_DESIGN_RATING(d->edit_obj) = option_n;
      cfedit_disp_menu(d);
    }
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

    // Check to see if we've already got a complex form with the same name.
    struct obj_data *form;
    for (form = asist->contains; form; form = form->next_content) {
      if ((isname(arg, form->text.keywords) || isname(arg, get_string_after_color_code_removal(form->restring, ch))) && GET_OBJ_TYPE(form) == ITEM_COMPLEX_FORM) {
        send_to_char(CH, "You already have a form with the name '%s', choose a new unique name.\r\n", arg);
        cfedit_disp_menu(d);
        return;
      }
    }

    DELETE_ARRAY_IF_EXTANT(d->edit_obj->restring);
    d->edit_obj->restring = str_dup(arg);
    cfedit_disp_menu(d);
    break;
  }
  case CFEDIT_WOUND:
    if (option_n < LIGHT || option_n > DEADLY)
      send_to_char(CH, "Not a valid option!\r\nEnter your choice: ");
    else {
      GET_DESIGN_PROGRAM_WOUND_LEVEL(d->edit_obj) = option_n;
      cfedit_disp_menu(d);
    }
    break;
  case CFEDIT_TYPE:
    if (option_n < 1 || option_n >= COMPLEX_FORM_TYPES)
      send_to_char(CH, "Not a valid option!\r\nEnter your choice: ");
    else {
      GET_DESIGN_PROGRAM(d->edit_obj) = complex_form_programs[option_n - 1];
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

ACMD(do_forms)
{
  struct obj_data *asist, *form;
  FAILURE_CASE(!(asist = find_cyberware(ch, CYB_ASIST)), "You don't have an asist converter.");

  char func[100];
  char *param = one_argument(argument, func);

  if (is_abbrev(func, "list") || strlen(func) <= 0) {
    strncpy(buf2, "", sizeof(buf2) - 1);
    for (form = asist->contains; form; form = form->next_content) {
      snprintf(ENDOF(buf2), sizeof(buf2) - strlen(buf2), " [%10s] %10s^n:^N %s R%d\r\n",
        GET_DESIGN_PROGRAMMING_TICKS_LEFT(form) <= 0 ? "^G  COMPLETE^n" : "^RINPROGRESS^n",
        form->restring,
        programs[GET_DESIGN_PROGRAM(form)].name,
        GET_DESIGN_RATING(form)
      );
      if (GET_DESIGN_PROGRAM(form) == SOFT_ATTACK)
        snprintf(ENDOF(buf2), sizeof(buf2) - strlen(buf2), "  Wound: %s\r\n", GET_WOUND_NAME(GET_DESIGN_PROGRAM_WOUND_LEVEL(form)));
    }
    send_to_char(ch, "Complex Forms:\r\n%s", buf2);
    return;
  }

  if (is_abbrev(func, "forget")) {
    skip_spaces(&param);
    for (form = asist->contains; form; form = form->next_content) {
      if ((isname(param, form->text.keywords) || isname(param, get_string_after_color_code_removal(form->restring, ch))) && GET_OBJ_TYPE(form) == ITEM_COMPLEX_FORM) 
        break;
    }
    if (!form) {
      send_to_char(ch, "The complex design isn't in your brain.\r\n");
      return;
    }
    send_to_char(ch, "You forget %s.\r\n", form->restring);
    extract_obj(form);
    return;
  }

  if (is_abbrev(func, "learn")) {
    if (!*param) {
      if (AFF_FLAGGED(ch, AFF_COMPLEX_FORM_PROGRAM)) {
        AFF_FLAGS(ch).RemoveBit(AFF_COMPLEX_FORM_PROGRAM);
        send_to_char(ch, "You stop working on %s.\r\n", ch->char_specials.programming->restring);
        ch->char_specials.programming = NULL;
      } else
        send_to_char(ch, "Meditate On What?\r\n");
      return;
    }

    skip_spaces(&param);
    for (form = asist->contains; form; form = form->next_content) {
      if ((isname(param, form->text.keywords) || isname(param, get_string_after_color_code_removal(form->restring, ch))) && GET_OBJ_TYPE(form) == ITEM_COMPLEX_FORM) 
        break;
    }
    if (!form) {
      send_to_char(ch, "The complex design isn't in your brain.\r\n");
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

    if (GET_DESIGN_PROGRAMMING_TICKS_LEFT(form) <= 0) {
      send_to_char(ch, "There's nothing more you can do with your %s complex form.", form->restring);
      return;
    }

    send_to_char(ch, "You continue to work on %s.\r\n", form->restring);
    AFF_FLAGS(ch).SetBit(AFF_COMPLEX_FORM_PROGRAM);
    GET_BUILDING(ch) = form;
    return;
  }

  send_to_char(ch, "Valid syntaxes: 'forms'/'forms list', 'forms learn <form>', 'forms forget <form>'.\r\n");
}