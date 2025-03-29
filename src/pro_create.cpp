// Sorry, but *snicker* - Che

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
#include "config.hpp"
#include "bullet_pants.hpp"
#include "dblist.hpp"
#include "memory.hpp"
#include "deck_build.hpp"
#include "ritualcast.hpp"
#include "metrics.hpp"
#include "matrix_storage.hpp"

#define CH d->character
#define PEDIT_MENU 0
#define PEDIT_TYPE 1
#define PEDIT_NAME 2
#define PEDIT_RATING 3
#define PEDIT_WOUND 4

extern void part_design(struct char_data *ch, struct obj_data *design);
extern void spell_design(struct char_data *ch, struct obj_data *design);
extern void complex_form_design(struct char_data *ch, struct obj_data *design);
extern void ammo_test(struct char_data *ch, struct obj_data *obj);
extern void weight_change_object(struct obj_data * obj, float weight);
extern bool focus_is_usable_by_ch(struct obj_data *focus, struct char_data *ch);

void pedit_disp_menu(struct descriptor_data *d)
{
  CLS(CH);
  send_to_char(CH, "1) Name: ^c%s^n\r\n", d->edit_matrix_file->name);
  send_to_char(CH, "2) Type: ^c%s^n\r\n", programs[d->edit_matrix_file->program_type].name);
  send_to_char(CH, "3) Rating: ^c%d^n\r\n", d->edit_matrix_file->rating);

  // Minimum program size is rating^2.
  int program_size = d->edit_matrix_file->rating ^ 2;
  if (d->edit_matrix_file->program_type == SOFT_ATTACK) {
    // Attack programs multiply size by a factor determined by wound level.
    program_size *= attack_multiplier[d->edit_matrix_file->wound_category];
    send_to_char(CH, "4) Damage: ^c%s^n\r\n", GET_WOUND_NAME(d->edit_matrix_file->wound_category));
  } else if (d->edit_matrix_file->program_type == SOFT_RESPONSE) {
      program_size = d->edit_matrix_file->rating ^ 3;
  } else {
    // Others multiply by a set multiplier based on software type.
    program_size *= programs[d->edit_matrix_file->program_type].multiplier;
  }
  send_to_char(CH, "\r\nInitial Design Size: ^c%d^n\r\n", (int) (program_size * 1.1));
  send_to_char(CH, "Completed Size: ^c%d^n\r\n\r\n", program_size);

  send_to_char(CH, "q) Quit and save\r\nEnter your choice: ");
  d->edit_mode = PEDIT_MENU;
}

void pedit_disp_program_menu(struct descriptor_data *d)
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
  d->edit_mode = PEDIT_TYPE;
}
void pedit_parse(struct descriptor_data *d, const char *arg)
{
  int number = atoi(arg);
  int program_size = 0;
  switch(d->edit_mode)
  {
  case PEDIT_MENU:
    switch (*arg) {
    case '1':
      send_to_char(CH, "What do you want to call this program: ");
      d->edit_mode = PEDIT_NAME;
      break;
    case '2':
      pedit_disp_program_menu(d);
      break;
    case '3':
      if (!d->edit_matrix_file->program_type)
        send_to_char("Choose a program type first!\r\n", CH);
      else {
        send_to_char("Enter Rating: ", CH);
        d->edit_mode = PEDIT_RATING;
      }
      break;
    case '4':
      if (d->edit_matrix_file->program_type != SOFT_ATTACK)
        send_to_char(CH, "Invalid option!\r\n");
      else {
        CLS(CH);
        send_to_char(CH, "1) Light\r\n2) Moderate\r\n3) Serious\r\n4) Deadly\r\nEnter your choice: ");
        d->edit_mode = PEDIT_WOUND;
      }
      break;
    case 'q':
    case 'Q':
      if (!d->edit_matrix_file->program_type || !d->edit_matrix_file->rating) {
        send_to_char("You must specify a program type and/or rating first.\r\n", CH);
        pedit_disp_menu(d);
        return;
      }

      send_to_char(CH, "Design saved!\r\n");
      // Minimum program size is rating^2.
      program_size = d->edit_matrix_file->rating ^ 2;
      if (d->edit_matrix_file->program_type == SOFT_ATTACK) {
        // Attack programs multiply size by a factor determined by wound level.
        program_size *= attack_multiplier[d->edit_matrix_file->wound_category];
      } else if (d->edit_matrix_file->program_type == SOFT_RESPONSE) {
        program_size = d->edit_matrix_file->rating ^ 3;
      } else {
        // Others multiply by a set multiplier based on software type.
        program_size *= programs[d->edit_matrix_file->program_type].multiplier;
      }
      
      d->edit_matrix_file->work_ticks_left = program_size;
      d->edit_matrix_file->size = program_size * 1.1;
      d->edit_matrix_file->original_size = program_size;
      d->edit_matrix_file->work_ticks_left  *= 20;
      d->edit_matrix_file->work_original_ticks_left = d->edit_matrix_file->work_ticks_left ;
      d->edit_matrix_file->creator_idnum = GET_IDNUM(CH);
      d->edit_matrix_file->work_phase = WORK_PHASE_READY;
      d->edit_matrix_file->dirty_bit = TRUE;
      adjust_device_memory(d->edit_matrix_file->in_obj, d->edit_matrix_file->size * -1);

      STATE(d) = CON_PLAYING;
      d->edit_matrix_file = NULL;
      break;
    default:
      send_to_char(CH, "Invalid option!\r\n");
      break;
    }
    break;
  case PEDIT_RATING:
    if (d->edit_matrix_file->program_type <= SOFT_SENSOR && number > GET_SKILL(CH, SKILL_COMPUTER) * 1.5)
      send_to_char(CH, "You can't create a persona program of a higher rating than your computer skill times one and a half.\r\n"
                   "Enter Rating: ");
    else if (d->edit_matrix_file->program_type == SOFT_EVALUATE && number > GET_SKILL(CH, SKILL_DATA_BROKERAGE))
      send_to_char(CH, "You can't create an evaluate program of a higher rating than your data brokerage skill.\r\n"
                   "Enter Rating: ");
    else if (d->edit_matrix_file->program_type > SOFT_SENSOR && number > GET_SKILL(CH, SKILL_COMPUTER))
      send_to_char(CH, "You can't create a program of a higher rating than your computer skill.\r\n"
                   "Enter Rating: ");
    else if (d->edit_matrix_file->program_type == SOFT_RESPONSE && number > 3)
      send_to_char("You can't create a response increase of a rating higher than 3.\r\nEnter Rating: ", CH);
    else if (number <= 0)
      send_to_char(CH, "You can't create a program with a rating less than 0..\r\n"
                   "Enter Rating: ");
    else {
      d->edit_matrix_file->rating = number;
      pedit_disp_menu(d);
    }
    break;
  case PEDIT_NAME:
  {
    int length_with_no_color = get_string_length_after_color_code_removal(arg, CH);

    // Silent failure: We already sent the error message in get_string_length_after_color_code_removal().
    if (length_with_no_color == -1) {
      pedit_disp_menu(d);
      return;
    }
    if (length_with_no_color >= LINE_LENGTH) {
      send_to_char(CH, "That name is too long, please shorten it. The maximum length after color code removal is %d characters.\r\n", LINE_LENGTH - 1);
      pedit_disp_menu(d);
      return;
    }

    if (strlen(arg) >= MAX_RESTRING_LENGTH) {
      send_to_char(CH, "That restring is too long, please shorten it. The maximum length with color codes included is %d characters.\r\n", MAX_RESTRING_LENGTH - 1);
      pedit_disp_menu(d);
      return;
    }

    d->edit_matrix_file->name = str_dup(arg);
    pedit_disp_menu(d);
    break;
  }
  case PEDIT_WOUND:
    if (number < LIGHT || number > DEADLY)
      send_to_char(CH, "Not a valid option!\r\nEnter your choice: ");
    else {
      d->edit_matrix_file->wound_category = number;
      pedit_disp_menu(d);
    }
    break;
  case PEDIT_TYPE:
    if (number < 1 || number >= NUM_PROGRAMS)
      send_to_char(CH, "Not a valid option!\r\nEnter your choice: ");
    else {
      d->edit_matrix_file->program_type = number;
      d->edit_matrix_file->rating = 1;

      if (d->edit_matrix_file->program_type == SOFT_ATTACK) {
        // Default to Deadly damage.
        d->edit_matrix_file->wound_category = DEADLY;
      }

      pedit_disp_menu(d);
    }
    break;
  }
}

struct obj_data *can_program(struct char_data *ch)
{
  struct obj_data *comp;
  if (IS_ASTRAL(ch))
    send_to_char("You can't read the screen in your current astral form.\r\n", ch);
  else if (IS_WORKING(ch))
    send_to_char("You are already working on something.\r\n", ch);
  else if (ch->in_veh && (ch->vfront || !ch->in_veh->flags.IsSet(VFLAG_WORKSHOP)))
    send_to_char("You can't do that in here.\r\n", ch);
  else {
    for (struct char_data *vict = ch->in_veh ? ch->in_veh->people : ch->in_room->people; vict; vict = vict->next_in_room)
      if (AFF_FLAGS(vict).AreAnySet(AFF_PROGRAM, AFF_DESIGN, ENDBIT)) {
        send_to_char(ch, "Someone is already using the computer.\r\n");
        return NULL;
      }
    FOR_ITEMS_AROUND_CH(ch, comp)
      if (GET_OBJ_TYPE(comp) == ITEM_DECK_ACCESSORY && GET_DECK_ACCESSORY_TYPE(comp) == TYPE_COMPUTER)
        break;
    if (ch->in_veh && comp && comp->vfront)
      comp = NULL;
    if (!comp) {
      send_to_char(ch, "There is no computer here for you to use.\r\n");
      return NULL;
    }
    return comp;
  }
  return NULL;
}

void create_program(struct char_data *ch)
{
  obj_data *deck = can_program(ch);
  if (!deck)
    return;

  struct matrix_file *design = create_matrix_file(deck, OBJ_LOAD_REASON_CREATE_PROGRAM);
  STATE(ch->desc) = CON_PRO_CREATE;
  design->name = str_dup("a blank program");
  design->file_type = MATRIX_FILE_DESIGN;
  ch->desc->edit_matrix_file = design;
  pedit_disp_menu(ch->desc);
}

int get_program_skill(char_data *ch, matrix_file *prog, int target)
{
  int skill = 0;
  switch (prog->program_type) {
  case SOFT_BOD:
  case SOFT_EVASION:
  case SOFT_MASKING:
  case SOFT_SENSOR:
  case SOFT_ASIST_COLD:
  case SOFT_ASIST_HOT:
  case SOFT_HARDENING:
  case SOFT_ICCM:
  case SOFT_ICON:
  case SOFT_MPCP:
  case SOFT_REALITY:
  case SOFT_RESPONSE:
    skill = get_skill(ch, SKILL_PROGRAM_CYBERTERM, target);
    break;
  case SOFT_ATTACK:
  case SOFT_SLOW:
    skill = get_skill(ch, SKILL_PROGRAM_COMBAT, target);
    break;
  case SOFT_CLOAK:
  case SOFT_LOCKON:
  case SOFT_MEDIC:
  case SOFT_ARMOR:
  case SOFT_SHIELD:
    skill = get_skill(ch, SKILL_PROGRAM_DEFENSIVE, target);
    break;
  case SOFT_BATTLETEC:
  case SOFT_COMPRESSOR:
  case SOFT_SLEAZE:
  case SOFT_TRACK:
  case SOFT_SUITE:
  case SOFT_RADIO:
    skill = get_skill(ch, SKILL_PROGRAM_SPECIAL, target);
    break;
  case SOFT_CAMO:
  case SOFT_CRASH:
  case SOFT_DEFUSE:
  case SOFT_EVALUATE:
  case SOFT_VALIDATE:
  case SOFT_SWERVE:
  case SOFT_SNOOPER:
  case SOFT_ANALYZE:
  case SOFT_DECRYPT:
  case SOFT_DECEPTION:
  case SOFT_RELOCATE:
  case SOFT_SCANNER:
  case SOFT_BROWSE:
  case SOFT_READ:
  case SOFT_COMMLINK:
    skill = get_skill(ch, SKILL_PROGRAM_OPERATIONAL, target);
    break;
  default:
    mudlog_vfprintf(ch, LOG_SYSLOG, "SYSERR: Unknown SOFT_X %d to do_design's switch statement!", prog->program_type);
    skill = 0;
    break;
  }
  return skill;
}

ACMD(do_design)
{
  ACMD_DECLARE(do_program);

  struct obj_data *comp, *proj;
  if (!*argument) {
    if (AFF_FLAGS(ch).AreAnySet(AFF_DESIGN, AFF_PROGRAM, ENDBIT)) {
      send_to_char(ch, "You stop working on %s.\r\n", GET_PROGRAMMING(ch)->name);
      STOP_WORKING(ch);
    } else if (AFF_FLAGS(ch).AreAnySet(AFF_SPELLDESIGN, ENDBIT)) {
      send_to_char(ch, "You stop working on %s.\r\n", GET_OBJ_NAME(GET_BUILDING(ch)));
      STOP_WORKING(ch);
    } else
      send_to_char(ch, "Design what?\r\n");
    return;
  }
  if (GET_POS(ch) > POS_SITTING) {
    GET_POS(ch) = POS_SITTING;
    send_to_char(ch, "You find a place to sit and work with your materials.\r\n");
  }
  if (IS_WORKING(ch)) {
    send_to_char(TOOBUSY, ch);
    return;
  }

  skip_spaces(&argument);
  proj = get_obj_in_list_vis(ch, argument, ch->carrying);
  if (proj) {
    if (GET_OBJ_TYPE(proj) == ITEM_PART) {
      part_design(ch, proj);
      return;
    } else if (GET_OBJ_TYPE(proj) == ITEM_SPELL_FORMULA) {
      spell_design(ch, proj);
      return;
    }
  }
  if (!(comp = can_program(ch)))
    return;
  skip_spaces(&argument);
  struct matrix_file *prog;
  prog = get_matrix_file_in_list_vis(ch, argument, comp->files);
  if (!prog) {
    send_to_char(ch, "The program design isn't on %s.\r\n", decapitalize_a_an(GET_OBJ_NAME(comp)));
    return;
  }

  if (prog->file_type != MATRIX_FILE_DESIGN || (prog->file_type == MATRIX_FILE_PROGRAM && prog->work_phase == WORK_PHASE_COMPLETE)) {
    send_to_char(ch, "Not much point in programming or designing something that's already complete.");
    return;
  } else if (prog->file_type == MATRIX_FILE_DESIGN && prog->work_phase == WORK_PHASE_COMPLETE) {
    send_to_char(ch, "There's no more design work to be done on %s, so you decide to try programming it instead.\r\n", prog->name);
    do_program(ch, argument, 0, 0);
    return;
  }

  int skill = 0, target = 4;
  if (prog->rating < 5)
    target--;
  else if (prog->rating > 9)
    target++;
  skill = get_program_skill(ch, prog, target);
  if (!skill) {
    send_to_char(ch, "You have no idea how to go about creating a program design for that.\r\n");
    return;
  }

  prog->work_phase = WORK_PHASE_IN_PROGRESS;
  prog->dirty_bit = TRUE;
  if (prog->work_original_ticks_left == prog->work_ticks_left) {
    if (get_and_deduct_one_crafting_token_from_char(ch)) {
      send_to_char("A crafting token fuzzes into digital static, greatly accelerating the design time.\r\n", ch);
      prog->work_successes = 10;
      prog->work_ticks_left = 1;
    }
    else if (access_level(ch, LVL_ADMIN)) {
      send_to_char(ch, "You use your admin powers to greatly accelerate the design time of %s.\r\n", prog->name);
      prog->work_successes = 10;
      prog->work_ticks_left = 1;
    } else {
      send_to_char(ch, "You begin designing %s.\r\n", prog->name);
      prog->work_successes = success_test(skill, target);
    }
  } else
    send_to_char(ch, "You continue to design %s.\r\n", prog->name);
  AFF_FLAGS(ch).SetBit(AFF_DESIGN);
  GET_PROGRAMMING(ch) = prog;
}

ACMD(do_program)
{
  struct obj_data *comp = NULL;
  struct matrix_file *prog = NULL;

  if (!*argument) {
    if (AFF_FLAGGED(ch, AFF_PROGRAM)) {
      AFF_FLAGS(ch).RemoveBit(AFF_PROGRAM);
      send_to_char(ch, "You stop working on %s.\r\n", ch->char_specials.programming->name);
      ch->char_specials.programming = NULL;
    } else
      send_to_char(ch, "Program What?\r\n");
    return;
  }
  if (GET_POS(ch) > POS_SITTING) {
    GET_POS(ch) = POS_SITTING;
    send_to_char(ch, "You find a place to sit and work.\r\n");
  }
  if (IS_WORKING(ch)) {
    send_to_char(TOOBUSY, ch);
    return;
  }
  if (!(comp = can_program(ch)))
    return;
  skip_spaces(&argument);
  prog = get_matrix_file_in_list_vis(ch, argument, comp->files);
  if (!prog) {
    send_to_char(ch, "The program design isn't on that computer.\r\n");
    return;
  }
  prog->dirty_bit = TRUE;
  if (!prog->work_ticks_left) {
    if (get_and_deduct_one_crafting_token_from_char(ch)) {
      send_to_char("A crafting token fuzzes into digital static, greatly accelerating the development time.\r\n", ch);
      prog->work_ticks_left = 1;
      prog->work_original_ticks_left = prog->work_ticks_left;
    }
    else if (access_level(ch, LVL_ADMIN)) {
      send_to_char(ch, "You use your admin powers to greatly accelerate the development time for %s.\r\n", prog->name);
      prog->work_ticks_left = 1;
      prog->work_original_ticks_left = prog->work_ticks_left;
    } else {
      send_to_char(ch, "You begin to program %s.\r\n", prog->name);
      int target = prog->rating;
      if (GET_DECK_ACCESSORY_COMPUTER_ACTIVE_MEMORY(comp) >= prog->rating * 2) {
        send_to_char(ch, "The abundance of active memory on %s makes the work easier.\r\n", decapitalize_a_an(GET_OBJ_NAME(comp)));
        target -= 2;
      }

      if (prog->file_type != MATRIX_FILE_DESIGN || prog->work_phase != WORK_PHASE_COMPLETE) {
        send_to_char("You haven't taken the time to design this program, so it's a little harder to conceptualize.\r\n", ch);
        target += 2;
      }
      else
        target -= prog->work_successes;
      
      int skill = get_skill(ch, SKILL_COMPUTER, target);
      int success = success_test(skill, target);
      for (struct obj_data *suite = comp->contains; suite; suite = suite->next_content)
        if (GET_OBJ_TYPE(suite) == ITEM_PROGRAM && GET_PROGRAM_TYPE(suite) == SOFT_SUITE) {
          success += (int)(success_test(MIN(GET_SKILL(ch, SKILL_COMPUTER), GET_PROGRAM_RATING(suite)), target) / 2);
          break;
        }
      if (success > 0) {
        prog->work_ticks_left = 60 * (prog->size / success);
        prog->work_original_ticks_left = prog->work_ticks_left;
      } else {
        prog->work_ticks_left = number(1, 6) + number(1, 6);
        prog->work_ticks_left = (prog->rating * 60) / number(1, 3);
        prog->work_successes = -1;
      }
    }
  } else
    send_to_char(ch, "You continue to work on %s.\r\n", prog->name);
  AFF_FLAGS(ch).SetBit(AFF_PROGRAM);
  GET_PROGRAMMING(ch) = prog;
}

#undef CH
#define CH desc->character
#define PROG GET_BUILDING(CH)

void clear_cyberdeck_part_build_data(struct obj_data *part) {
  GET_PART_BUILDER_IDNUM(part) = 0;
  GET_PART_BUILD_SUCCESSES_ROLLED(part) = 0;
  GET_PART_BUILD_TICKS_REMAINING(part) = 0;
  GET_PART_INITIAL_BUILD_TICKS(part) = 0;

  if (part->cyberdeck_part_pointer)
    clear_cyberdeck_part_pointer(part);
}

void update_buildrepair(void)
{
  PERF_PROF_SCOPE(pr_, __func__);
  struct descriptor_data *next;
  for (struct descriptor_data *desc = descriptor_list; desc; desc = next) {
    next = desc->next;
    if (CH && STATE(desc) == CON_PLAYING) {
      if (AFF_FLAGGED(desc->character, AFF_PART_BUILD)) {
        if (--GET_PART_BUILD_TICKS_REMAINING(PROG) < 1) {
          if (GET_PART_BUILD_SUCCESSES_ROLLED(PROG) < 0) {
            send_to_char(desc->character, "A crucial component snaps in your hands-- you botched the installation and accidentally destroyed %s!\r\n", GET_OBJ_NAME(PROG));
            clear_cyberdeck_part_build_data(PROG);
            extract_obj(PROG);
          } else if (GET_PART_BUILD_SUCCESSES_ROLLED(PROG) == 0) {
            send_to_char(CH, "You realize you messed up somewhere in the process of installing %s, but at least it's salvagable.\r\n", GET_OBJ_NAME(PROG));
            clear_cyberdeck_part_build_data(PROG);
          } else {
            // Sanity checks.
            if (!PROG->cyberdeck_part_pointer) {
              send_to_char("Something went wrong-- your part has no deck to install into! Aborting.\r\n", CH);
              mudlog("SYSERR: Cyberdeck part has nothing to install into!!", CH, LOG_SYSLOG, TRUE);
              clear_cyberdeck_part_build_data(PROG);
              STOP_WORKING(desc->character);
              continue;
            }
            if (!part_is_compatible_with_deck(PROG, PROG->cyberdeck_part_pointer, CH)) {
              send_to_char("With that realization, you stop working on %s.\r\n", CH);
              mudlog_vfprintf(CH, LOG_CHEATLOG, "SYSERR: Hit incompatible part in deckbuilding process in final sanity check. Look into this for potential exploiting. (%s failed to install %s-%d '%s^n' (target MPCP %d) into MPCP-%d '%s^n')",
                              GET_CHAR_NAME(CH), 
                              parts[GET_PART_TYPE(PROG)].name, 
                              GET_PART_RATING(PROG), 
                              GET_OBJ_NAME(PROG),
                              GET_PART_TARGET_MPCP(PROG),
                              GET_CYBERDECK_MPCP(PROG->cyberdeck_part_pointer),
                              GET_OBJ_NAME(PROG->cyberdeck_part_pointer));
              clear_cyberdeck_part_pointer(PROG);
              GET_PART_BUILDER_IDNUM(PROG) = 0; // Wipe out the builder's idnum so it can be worked on by someone else.
              STOP_WORKING(CH);
              continue;
            }
            send_to_char(desc->character, "You finish installing %s into %s.\r\n", GET_OBJ_NAME(PROG), GET_OBJ_NAME(PROG->cyberdeck_part_pointer));
            CH->char_specials.timer = 0;
            obj_from_char(PROG);
            obj_to_obj(PROG, PROG->cyberdeck_part_pointer);
            clear_cyberdeck_part_build_data(PROG);

            GET_PART_BUILD_TICKS_REMAINING(PROG) = -2;
            if (!GET_CYBERDECK_MPCP(PROG->in_obj))
              GET_CYBERDECK_MPCP(PROG->in_obj) = GET_PART_TARGET_MPCP(PROG);

            switch(GET_PART_TYPE(PROG)) {
              case PART_MPCP:
                GET_CYBERDECK_MPCP(PROG->in_obj) = GET_PART_RATING(PROG);
                break;
              case PART_RESPONSE:
                GET_CYBERDECK_RESPONSE_INCREASE(PROG->in_obj) = GET_PART_RATING(PROG);
                break;
              case PART_IO:
                GET_CYBERDECK_IO_RATING(PROG->in_obj) = GET_PART_RATING(PROG);
                break;
              case PART_STORAGE:
                GET_CYBERDECK_TOTAL_STORAGE(PROG->in_obj) = GET_PART_RATING(PROG);
                break;
              case PART_HARDENING:
                GET_CYBERDECK_HARDENING(PROG->in_obj) = GET_PART_RATING(PROG);
                break;
              case PART_ACTIVE:
                GET_CYBERDECK_ACTIVE_MEMORY(PROG->in_obj) = GET_PART_RATING(PROG);
                break;
            }
            if (GET_CYBERDECK_IS_INCOMPLETE(PROG->in_obj)) {
              int x = 0;
              for (struct obj_data *obj = PROG->in_obj->contains; obj; obj = obj->next_content)
                if (GET_OBJ_TYPE(obj) == ITEM_PART)
                  switch (GET_PART_TYPE(PROG)) {
                    case PART_MPCP:
                    case PART_ACTIVE:
                    case PART_BOD:
                    case PART_SENSOR:
                    case PART_IO:
                    case PART_MATRIX_INTERFACE:
                      x++;
                      break;
                  }
              if (x == 6)
                GET_CYBERDECK_IS_INCOMPLETE(PROG->in_obj) = 0;
            }
          }
          STOP_WORKING(desc->character);
        }
      } else if (AFF_FLAGGED(desc->character, AFF_PART_DESIGN) && --GET_PART_DESIGN_COMPLETION(PROG) < 1) {
        send_to_char(desc->character, "You complete the design plan for %s.\r\n", GET_OBJ_NAME(PROG));
        CH->char_specials.timer = 0;
        STOP_WORKING(CH);
      } else if (AFF_FLAGGED(desc->character, AFF_DESIGN)) {
        GET_PROGRAMMING(CH)->dirty_bit = TRUE;
        if (--GET_PROGRAMMING(CH)->work_ticks_left < 1) {
          send_to_char(desc->character, "You complete the design plan for %s.\r\n", GET_PROGRAMMING(CH)->name);
          GET_PROGRAMMING(CH)->work_phase = WORK_PHASE_COMPLETE;
          GET_PROGRAMMING(CH) = NULL;
          AFF_FLAGS(desc->character).RemoveBit(AFF_DESIGN);
        }
      } else if (AFF_FLAGGED(desc->character, AFF_PROGRAM)) {
        GET_PROGRAMMING(CH)->dirty_bit = TRUE;
        if (--GET_PROGRAMMING(CH)->work_ticks_left < 1) {
          if (GET_PROGRAMMING(CH)->work_successes < 0) {
            switch(number(1,10)) {
              case 1:
                send_to_char(desc->character, "It was about that time that you noticed you had typed up all of your code on the microwave keypad. You realise programming %s is a lost cause.\r\n", GET_PROGRAMMING(CH)->name);
                break;
              case 2:
                send_to_char(desc->character, "There was a series of articles related to what you were doing, but you somehow ended up on a page about crabs. Why always crabs?!? You realise programming %s is a lost cause.\r\n", GET_PROGRAMMING(CH)->name);
                break;
              case 3:
                send_to_char(desc->character, "You became distracted and lost hours of your life to a Penumbrawalk mud. You realise programming %s is a lost cause.\r\n", GET_PROGRAMMING(CH)->name);
                break;
              case 4:
                send_to_char(desc->character, "You've finally done it! You've made an electric drum kit out of tin foil and pen parts! Programming %s failed though.\r\n", GET_PROGRAMMING(CH)->name);
                break;
              case 5:
                send_to_char(desc->character, "You've been banned from the ShadowBoards. You realise programming %s is a lost cause.\r\n", GET_PROGRAMMING(CH)->name);
                break;
              case 6:
                send_to_char(desc->character, "You have finished a spell formula for Stunbolt... wait, what the hell?! You realise programming %s is a lost cause.\r\n", GET_PROGRAMMING(CH)->name);
                break;
              case 7:
                send_to_char(desc->character, "A distant, powerful matrix entity is disappointed in you. You realise programming %s is a lost cause.\r\n", GET_PROGRAMMING(CH)->name);
                break;
              case 8:
                send_to_char(desc->character, "You tried to forge %s into an incredible program that would have pierced open the walls of flowing code. You're pretty sure you misplaced a parenthesis somewhere so it all turned into gibberish about broccoli.\r\n", GET_PROGRAMMING(CH)->name);
                break;
              case 9:
                send_to_char(desc->character, "You tried to program %s only to realize many hours in you were coding on one of the spare half-assembled things you had littered around your current workspace, accomplishing nothing.\r\n", GET_PROGRAMMING(CH)->name);
                break;
              default:
                send_to_char(desc->character, "You realise programming %s is a lost cause.\r\n", GET_PROGRAMMING(CH)->name);
              }
              // Delete the design file as it's no longer needed.
              extract_matrix_file(GET_PROGRAMMING(CH));
            }
          else {
            send_to_char(desc->character, "You complete programming %s.\r\n", GET_PROGRAMMING(CH)->name);
            // We can just change the existing design file into a finished program, rather than dupe it.
            GET_PROGRAMMING(CH)->work_phase = WORK_PHASE_COMPLETE;
            adjust_device_memory(GET_PROGRAMMING(CH)->in_obj, GET_PROGRAMMING(CH)->size - GET_PROGRAMMING(CH)->original_size);
            GET_PROGRAMMING(CH)->size = GET_PROGRAMMING(CH)->original_size;
            GET_PROGRAMMING(CH)->original_size = 0;
            GET_PROGRAMMING(CH)->file_type = MATRIX_FILE_SOURCE_CODE;
            if (GET_PROGRAMMING(CH)->program_type == SOFT_SUITE)
              GET_PROGRAMMING(CH)->file_type = MATRIX_FILE_PROGRAM; // Programming suites can just be programs
          }
          GET_PROGRAMMING(CH) = NULL;
          AFF_FLAGS(desc->character).RemoveBit(AFF_PROGRAM);
          CH->char_specials.timer = 0;
        }
      } else if (AFF_FLAGGED(desc->character, AFF_COMPLEX_FORM_PROGRAM)) {
        if (--GET_COMPLEX_FORM_LEARNING_TICKS_LEFT(PROG) < 1) {
          if (GET_COMPLEX_FORM_LEARNING_FAILED(PROG)) {
            switch(number(1,10)) {
              case 1:
                send_to_char(desc->character, "You have finished a spell formula for Stunbolt... wait, what the hell?! You realise learning %s is a lost cause.\r\n", GET_OBJ_NAME(PROG));
                break;
              case 2:
                send_to_char(desc->character, "There was a series of articles related to what you were doing, but you somehow ended up on a page about crabs. Why always crabs?!? You realise learning %s is a lost cause.\r\n", GET_OBJ_NAME(PROG));
                break;
              case 3:
                send_to_char(desc->character, "You became distracted and lost hours of your life to a Penumbrawalk mud. You realise learning %s is a lost cause.\r\n", GET_OBJ_NAME(PROG));
                break;
              case 4:
                send_to_char(desc->character, "You try to shape the resonance but somehow end up ordering takeout from a Stuffer Shack in Neo-Tokyo. You realise learning %s is a lost cause.\r\n", GET_OBJ_NAME(PROG));
                break;
              case 5:
                send_to_char(desc->character, "The deep resonance speaks to you! Unfortunately, it's just complaining about matrix lag. You realise learning %s is a lost cause.\r\n", GET_OBJ_NAME(PROG));
                break;
              case 6:
                send_to_char(desc->character, "You reach for the threads of resonance but accidentally subscribe to 47 different spam newsletters. You realise learning %s is a lost cause.\r\n", GET_OBJ_NAME(PROG));
                break;
              case 7:
                send_to_char(desc->character, "You achieve deep matrix meditation, only to discover you've been watching cat videos for the last four hours. You realise learning %s is a lost cause.\r\n", GET_OBJ_NAME(PROG));
                break;
              case 8:
                send_to_char(desc->character, "Success! You've mastered... wait, no, that's just static cling. You realise learning %s is a lost cause.\r\n", GET_OBJ_NAME(PROG));
                break;
              case 9:
                send_to_char(desc->character, "Your attempt to weave the resonance somehow results in all nearby devices playing 'Never Gonna Give You Up'. You realise learning %s is a lost cause.\r\n", GET_OBJ_NAME(PROG));
                break;
              default:
                send_to_char(desc->character, "You realise learning %s is a lost cause.\r\n", GET_OBJ_NAME(PROG));
            }
            extract_obj(PROG);
          } else {
            send_to_char(desc->character, "You successfully learn %s.\r\n", GET_OBJ_NAME(PROG));
            GET_COMPLEX_FORM_LEARNING_TICKS_LEFT(PROG) = 0;            
          }
          PROG = NULL;
          AFF_FLAGS(desc->character).RemoveBit(AFF_COMPLEX_FORM_PROGRAM);
        }
      } else if (AFF_FLAGGED(CH, AFF_RITUALCAST)) {
        if (handle_ritualcast_tick(CH, PROG)) {
          // Successful completion means the prog was extracted in the function.
          PROG = NULL;
        }
      } else if (AFF_FLAGGED(CH, AFF_BONDING)) {
        if ((GET_OBJ_TYPE(PROG) == ITEM_WEAPON && --GET_WEAPON_FOCUS_BOND_STATUS(PROG) < 1)
            || (GET_OBJ_TYPE(PROG) == ITEM_FOCUS && --GET_FOCUS_BOND_TIME_REMAINING(PROG) < 1))
        {
          send_to_char(CH, "You complete the bonding ritual for %s.\r\n", GET_OBJ_NAME(PROG));
          CH->char_specials.timer = 0;
          STOP_WORKING(CH);
        }
      } else if (AFF_FLAGGED(CH, AFF_CONJURE) && --CH->char_specials.conjure[2] < 1) {
        if ((GET_OBJ_COST(PROG) -= CH->char_specials.conjure[1] * 1000) <= 0)
          extract_obj(PROG);
        STOP_WORKING(CH);
        int skill = GET_SKILL(CH, SKILL_CONJURING);
        int target = CH->char_specials.conjure[1];
        int type = CH->char_specials.conjure[0];
        aspect_conjuring_bonus(CH, CONJURING, type, target, skill);

        char rollbuf[5000];
        snprintf(rollbuf, sizeof(rollbuf), "Conjure check: initial skill %d, initial target %d", skill, target);

        // Modify the skill rating by foci.
        bool used_spirit_focus = FALSE, used_power_focus = FALSE;
        for (int i = 0; i < NUM_WEARS; i++) {
          struct obj_data *eq = GET_EQ(CH, i);

          if (!eq || GET_OBJ_TYPE(eq) != ITEM_FOCUS)
            continue;

          if (!used_spirit_focus
              && GET_FOCUS_TYPE(eq) == FOCI_SPIRIT
              && focus_is_usable_by_ch(eq, CH)
              && GET_FOCUS_BONDED_SPIRIT_OR_SPELL(eq) == CH->char_specials.conjure[0]
              && GET_FOCUS_ACTIVATED(eq))
          {
            used_spirit_focus = TRUE;
            skill += GET_FOCUS_FORCE(eq);
            snprintf(ENDOF(rollbuf), sizeof(rollbuf) - strlen(rollbuf), ", +%d skill from spirit focus", GET_FOCUS_FORCE(eq));
          }

          else if (!used_power_focus
                   && GET_FOCUS_TYPE(eq) == FOCI_POWER
                   && focus_is_usable_by_ch(eq, CH)
                   && GET_FOCUS_ACTIVATED(eq))
          {
            used_power_focus = TRUE;
            skill += GET_FOCUS_FORCE(eq);
            snprintf(ENDOF(rollbuf), sizeof(rollbuf) - strlen(rollbuf), ", +%d skill from power focus", GET_FOCUS_FORCE(eq));
          }
        }

        if (GET_BACKGROUND_AURA(CH->in_room) == AURA_POWERSITE) {
          skill += GET_BACKGROUND_COUNT(CH->in_room);
          snprintf(ENDOF(rollbuf), sizeof(rollbuf) - strlen(rollbuf), ", +%d skill from power site", GET_BACKGROUND_COUNT(CH->in_room));
        } else {
          target += GET_BACKGROUND_COUNT(CH->in_room);
          snprintf(ENDOF(rollbuf), sizeof(rollbuf) - strlen(rollbuf), ", +%d TN from background count", GET_BACKGROUND_COUNT(CH->in_room));
        }

        int success = success_test(skill, target);
        snprintf(ENDOF(rollbuf), sizeof(rollbuf) - strlen(rollbuf), ". Rolled %d successes.\r\n", success);
        act(rollbuf, FALSE, CH, NULL, NULL, TO_ROLLS);

        if (success < 1) {
          send_to_char(CH, "You fail to conjure the %s elemental.\r\n", elements[CH->char_specials.conjure[0]].name);
          continue;
        }

        send_to_char(CH, "You summon a %s elemental.\r\n", elements[CH->char_specials.conjure[0]].name);

        if (conjuring_drain(CH, CH->char_specials.conjure[1])) {
          // Conjurer died. Elemental is not viable.
          continue;
        }

        int idnum = number(0, 1000);
        struct char_data *mob = create_elemental(CH, CH->char_specials.conjure[0], CH->char_specials.conjure[1], idnum, TRAD_HERMETIC);
        act("$n appears just outside of the hermetic circle.", FALSE, mob, 0, 0, TO_ROOM);

        if (!AWAKE(CH)) {
          if (success_test(GET_LEVEL(mob), 6) > 0)
            extract_char(mob);
          else {
            GET_ACTIVE(mob) = 0;
            MOB_FLAGS(mob).SetBit(MOB_AGGRESSIVE);
            stop_follower(mob);
          }
          continue;
        }
        struct spirit_data *spirit = new spirit_data;
        spirit->type = CH->char_specials.conjure[0];
        spirit->force = CH->char_specials.conjure[1];
        spirit->services = success;
        spirit->id = idnum;
        spirit->next = GET_SPIRIT(CH);
        spirit->called = TRUE;
        GET_SPIRIT(CH) = spirit;
        GET_NUM_SPIRITS(CH)++;
        GET_ELEMENTALS_DIRTY_BIT(CH) = TRUE;
        GET_SPARE2(mob) = spirit->id;
      } else if (AFF_FLAGGED(CH, AFF_CIRCLE) && --GET_MAGIC_TOOL_BUILD_TIME_LEFT(PROG) < 1) {
        send_to_char("You complete drawing the circle.\r\n", CH);
        act("$n finishes drawing the hermetic circle.\r\n", FALSE, CH, 0, 0, TO_ROOM);
        CH->char_specials.timer = 0;
        STOP_WORKING(CH);
      } else if (AFF_FLAGGED(CH, AFF_LODGE) && --GET_MAGIC_TOOL_BUILD_TIME_LEFT(PROG) < 1) {
        send_to_char("You finish building the lodge.\r\n", CH);
        act("$n finishes building the lodge.\r\n", FALSE, CH, 0, 0, TO_ROOM);
        CH->char_specials.timer = 0;
        STOP_WORKING(CH);
      } else if (AFF_FLAGGED(CH, AFF_AMMOBUILD) && --GET_AMMOBOX_TIME_TO_COMPLETION(PROG) < 1) {
        if (GET_AMMOBOX_TIME_TO_COMPLETION(PROG) <= -2) { // --(-1) = -2; prevents penalizing people who ace the test.
          switch(number(1,18)) {
            case 1:
              send_to_char("You swear loudly as a gunshot rings out because you accidentally fired the round.\r\n", CH);
              break;
            case 2:
              send_to_char("The poor-quality metal shavings you're trying to turn into a bullet disintegrate and you curse the entire lineage of the fixer who gave you these materials.\r\n", CH);
              break;
            case 3:
              send_to_char("Somehow, an ancient unknown language comes unbidden to your lips. You think you said something unspeakable about a dog - regardless, the bullet is now tainted. You discard it.\r\n", CH);
              break;
            case 4:
              send_to_char("You grind and grind and grind and grind and grind and grind and then you realize you have nothing left to grind.\r\n", CH);
              break;
            case 5:
              send_to_char("You drop a tool, causing a spark to light from your gunpowder to the casing you're trying to pack. There's a disappointing 'pop' noise as it bursts into flames.\r\n", CH);
              break;
            case 6:
              send_to_char("You're pretty sure the bullet started talking to you. After several hours, things have started to blur together - better be safe, though. It might be a free spirit. You throw it away.\r\n", CH);
              break;
            case 7:
              send_to_char("You were trying to craft APDS, weren't you? Poor bastard.\r\n", CH);
              break;
            case 8:
              send_to_char("You stumble slightly, jamming your thumb in the vice. The process of ripping your hand out obliterates the shell casing.\r\n", CH);
              break;
            case 9:
              send_to_char("You decide to use a hammer to knock the back of the casing up onto the bullet... BAM! Now that wasn't very smart, was it?\r\n", CH);
              break;
            default:
              send_to_char("You seem to have messed up this batch of ammo.\r\n", CH);
          }
        }
        else {
          send_to_char("You have completed a batch of ammo.\r\n", CH);
          GET_AMMOBOX_QUANTITY(PROG) += AMMOBUILD_BATCH_SIZE;
          AMMOTRACK_OK(GET_AMMOBOX_WEAPON(PROG), GET_AMMOBOX_TYPE(PROG), AMMOTRACK_CRAFTING, AMMOBUILD_BATCH_SIZE);

          // Add the weight of the completed ammo to the box. Due to some ammo having miniscule weight, we must fully re-calc every time.
          weight_change_object(PROG, -GET_OBJ_WEIGHT(PROG));
          weight_change_object(PROG, get_ammo_weight(GET_AMMOBOX_WEAPON(PROG), GET_AMMOBOX_TYPE(PROG), GET_AMMOBOX_QUANTITY(PROG), CH));
        }
        GET_AMMOBOX_INTENDED_QUANTITY(PROG) -= AMMOBUILD_BATCH_SIZE;
        if (GET_AMMOBOX_INTENDED_QUANTITY(PROG) <= 0) {
          send_to_char(CH, "You have finished building %s.\r\n", GET_OBJ_NAME(PROG));
          STOP_WORKING(CH);
        } else ammo_test(CH, PROG);
      } else if (AFF_FLAGGED(CH, AFF_SPELLDESIGN) && --GET_SPELLFORMULA_TIME_LEFT(PROG) < 1) {
        if (GET_OBJ_TIMER(PROG) == SPELL_DESIGN_FAILED_CODE) {
          switch(number(1,8)) {
            case 1:
              send_to_char(CH, "The Dweller on the Threshold notices your attempts at spell creation and laughs. You failed to design %s.\r\n", GET_OBJ_NAME(PROG));
              break;
            case 2:
              send_to_char(CH, "This spell would have been the profane bridge that brought the Horrors to the Sixth World. However, they were thoroughly unimpressed with your work. You failed to design %s.\r\n", GET_OBJ_NAME(PROG));
              break;
            case 3:
              send_to_char(CH, "You draw the Magus of the Eternal Gods, Lord of the Wild and Fertile Lands, and the Ten of Spades. Go fish. You failed to design %s.\r\n", GET_OBJ_NAME(PROG));
              break;
            case 4:
              send_to_char(CH, "You finished programming a Carrot Top reality filter for your cyberdeck - wait, what?!? You realise you have lost your inspiration for %s\r\n", GET_OBJ_NAME(PROG));
              break;
            case 5:
              send_to_char(CH, "You've spilt your ritual chalice of your favorite drink all over %s! So much for that spell!\r\n", GET_OBJ_NAME(PROG));
              break;
            default:
              send_to_char(CH, "You realise you have lost your inspiration for %s.\r\n", GET_OBJ_NAME(PROG));
          }
          send_to_char(CH, "With your work wasted, you trash the unfinishable %s.\r\n", GET_OBJ_NAME(PROG));
          extract_obj(PROG);
        } else {
          send_to_char(CH, "You successfully finish designing %s.\r\n", GET_OBJ_NAME(PROG));
          CH->char_specials.timer = 0;
          GET_OBJ_TIMER(PROG) = 0;
        }
        STOP_WORKING(CH);
      }
    }
  }
}
