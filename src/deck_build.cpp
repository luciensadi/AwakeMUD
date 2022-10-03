#include <stdlib.h>

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
#include "newmatrix.hpp"

#define CH d->character
#define PART d->edit_obj
#define DEDIT_MAIN 0
#define DEDIT_TYPE 1
#define DEDIT_MPCP 2
#define DEDIT_NAME 3
#define DEDIT_RATING 4

extern void ammo_build(struct char_data *ch, struct obj_data *obj);

ACMD_DECLARE(do_sit);

bool part_is_nerps(int part_type) {
  switch (part_type) {
    case PART_PORTS:
    case PART_MASER:
    case PART_CELLULAR:
    case PART_LASER:
    case PART_MICROWAVE:
    case PART_RADIO:
    case PART_SATELLITE:
    case PART_SIGNAL_AMP:
      return TRUE;
  }

  return FALSE;
}

bool part_can_have_its_rating_set(struct obj_data *part) {
  switch (GET_PART_TYPE(part)) {
    case PART_RESPONSE:
    case PART_HARDENING:
    case PART_STORAGE:
    case PART_ACTIVE:
    case PART_ICON:
    case PART_BOD:
    case PART_SENSOR:
    case PART_MASKING:
    case PART_EVASION:
    case PART_RADIO:
    case PART_CELLULAR:
    case PART_SATELLITE:
    case PART_IO:
      return TRUE;
    default:
      return FALSE;
  }
}

int get_part_maximum_rating(struct obj_data *part) {
  int mpcp = GET_PART_TARGET_MPCP(part);
  int part_type = GET_PART_TYPE(part);

  switch (part_type) {
    case PART_IO:
      return mpcp * 100;
    case PART_STORAGE:
      return mpcp * 600;
    case PART_ACTIVE:
      return mpcp * 250;
    case PART_RESPONSE:
      return MIN(3, (int)(mpcp / 4));
    case PART_HARDENING:
    case PART_ICON:
    case PART_BOD:
    case PART_SENSOR:
    case PART_MASKING:
    case PART_EVASION:
    case PART_RADIO:
    case PART_CELLULAR:
    case PART_SATELLITE:
      return mpcp;
    case PART_ASIST_HOT:
    case PART_RAS_OVERRIDE:
      return 0;
  }

  char oopsbuf[1000];
  snprintf(oopsbuf, sizeof(oopsbuf), "SYSERR: Received unknown part type %d to get_part_maximum_rating()!", part_type);
  mudlog(oopsbuf, NULL, LOG_SYSLOG, TRUE);
  return -1;
}

int get_part_cost(int type, int rating, int mpcp) {
    switch (type) {
    case PART_STORAGE:
        return (int)(rating * 0.5);
    case PART_ACTIVE:
        return (int)(rating * 1.5);
    case PART_ASIST_HOT:
        return (25 * (mpcp*mpcp)) + 1250;
    case PART_ASIST_COLD:
        return (25 * mpcp) + 1250;
    case PART_HARDENING:
        return 25 * (rating*rating);
    case PART_ICCM:
        return (35 * mpcp) + 1000;
    case PART_ICON:
    case PART_BOD:
    case PART_SENSOR:
    case PART_MASKING:
    case PART_EVASION:
    case PART_CELLULAR:
    case PART_RADIO:
    case PART_SATELLITE:
        return 35 * (rating * rating);
    case PART_MPCP:
        return 35 * (mpcp * mpcp);
    case PART_IO:
        return 35 * (rating / 10);
    case PART_MASER:
        return 3000;
    case PART_MATRIX_INTERFACE:
        return 37;
    case PART_PORTS:
        return 235;
    case PART_RAS_OVERRIDE:
        return (35 * mpcp) + 1000;
    case PART_REALITY_FILTER:
        return 70 * mpcp;
    case PART_RESPONSE:
        return 135 * rating;
    case PART_SIGNAL_AMP:
        return 285;
    case PART_LASER:
    case PART_MICROWAVE:
        return 560;
    }
    return 0;
}

int get_chip_cost(int type, int rating, int mpcp) {
    int ratingcost = (int)(rating * rating * programs[parts[type].software].multiplier * 0.5);
    int mpcpcost = (int)(mpcp * mpcp * programs[parts[type].software].multiplier * 0.5);
    switch (type) {
    case PART_ACTIVE:
        return rating * 2;
    case PART_STORAGE:
        return rating;
    case PART_ASIST_HOT:
    case PART_ASIST_COLD:
    case PART_ICCM:
    case PART_BOD:
    case PART_SENSOR:
    case PART_MASKING:
    case PART_EVASION:
    case PART_REALITY_FILTER:
    case PART_MPCP:
        return mpcpcost;
    case PART_ICON:
    case PART_HARDENING:
        return ratingcost;
    case PART_RESPONSE:
        return (mpcp*mpcp) * (rating*2);
    }
    return 0;
}

// Enforce certain restrictions on values in parts. Why it was done here instead of in the setter logic is anyone's guess.
void enforce_restrictions(struct obj_data *part) {
  if (GET_PART_TYPE(part) == PART_RESPONSE)
    GET_PART_TARGET_MPCP(part) = MAX(3, GET_PART_TARGET_MPCP(part));
}

void partbuild_main_menu(struct descriptor_data *d) {
  CLS(CH);
  enforce_restrictions(PART);

  send_to_char(CH, "1) Name: ^c%-40s^n     Software Needed: ^c%s^n\r\n",
               PART->restring, YESNO(parts[GET_PART_TYPE(PART)].software));
  send_to_char(CH, "2) Type: ^c%-40s^n     Parts Cost: ^c%d nuyen^n\r\n",
               parts[GET_PART_TYPE(PART)].name, get_part_cost(GET_PART_TYPE(PART), GET_PART_RATING(PART), GET_PART_TARGET_MPCP(PART)));
  send_to_char(CH, "3) MPCP Designed For: ^c%-30d^n  Chips Cost: ^c%d nuyen^n\r\n",
               GET_PART_TARGET_MPCP(PART),
               get_chip_cost(GET_PART_TYPE(PART), GET_PART_RATING(PART), GET_PART_TARGET_MPCP(PART)));
  if (GET_PART_TARGET_MPCP(PART) && part_can_have_its_rating_set(PART))
    send_to_char(CH, "4) %s: ^c%d^n\r\n",
                GET_OBJ_VAL(d->edit_obj, 0) == PART_STORAGE || GET_OBJ_VAL(d->edit_obj, 0) == PART_ACTIVE ? "Capacity" : "Rating",
                GET_PART_RATING(PART));
  send_to_char(CH, "q) Save and Quit\r\n");
  send_to_char(CH, "Enter Option: ");
  d->edit_mode = DEDIT_MAIN;
}

void render_part_type_to_character(struct char_data *ch, int index, int part_type, bool newline) {
  char type_string[1000];
  strlcpy(type_string, parts[part_type].name, sizeof(type_string));

  if (part_is_nerps(part_type)) {
    strlcat(type_string, " (Not Implemented)", sizeof(type_string));
  }
  send_to_char(ch, "%2d) %-40s%s", index, type_string, newline ? "\r\n" : "");
}

void partbuild_disp_types(struct descriptor_data *d) {
  CLS(CH);
  for (int x = 1; x < NUM_PARTS; x++)
    render_part_type_to_character(CH, x, x, D_PRF_FLAGGED(d, PRF_SCREENREADER) ? TRUE : x % 2 == 0);
  send_to_char(CH, "\r\nEnter Part Number: ");
  d->edit_mode = DEDIT_TYPE;
}

void pbuild_parse(struct descriptor_data *d, const char *arg) {
    int number = atoi(arg);
    switch (d->edit_mode) {
    case DEDIT_MAIN:
        switch (*arg) {
        case '1':
            send_to_char(CH, "What do you want to call this part: ");
            d->edit_mode = DEDIT_NAME;
            break;
        case '2':
            partbuild_disp_types(d);
            break;
        case '3':
            if (parts[GET_PART_TYPE(PART)].design >= 0 ||
                    GET_PART_TYPE(PART) == PART_ACTIVE || GET_PART_TYPE(PART) == PART_STORAGE || GET_PART_TYPE(PART) == PART_MATRIX_INTERFACE) {
                send_to_char(CH, "MPCP of Target Deck: ");
                d->edit_mode = DEDIT_MPCP;
            } else
                send_to_char(CH, "That is not needed for this part. Enter Option: ");
            break;
        case 'q':
        case 'Q':
            // Set design requirements (if needed)
            if (parts[GET_PART_TYPE(PART)].design == -1) {
                GET_PART_DESIGN_COMPLETION(PART) = 0;
            } else {
                GET_PART_DESIGN_COMPLETION(PART) = -1;
            }

            // ?
            GET_OBJ_VAL(PART, 4) = -1;

            // True up part rating to MPCP if it's been nulled.
            if (!GET_PART_RATING(PART))
                GET_PART_RATING(PART) = GET_PART_TARGET_MPCP(PART);

            // Set the cost.
            GET_PART_CHIP_COST(PART) = get_chip_cost(GET_PART_TYPE(PART), GET_PART_RATING(PART), GET_PART_TARGET_MPCP(PART));
            GET_PART_PART_COST(PART) = get_part_cost(GET_PART_TYPE(PART), GET_PART_RATING(PART), GET_PART_TARGET_MPCP(PART));

            // Hand it over.
            obj_to_char(PART, CH);
            PART = NULL;
            STATE(d) = CON_PLAYING;
            send_to_char(CH, "Design Saved!\r\n");
            break;
        case '4':
            if (part_can_have_its_rating_set(PART)) {
              switch (GET_PART_TYPE(PART)) {
                case PART_IO:
                  send_to_char(CH, "I/O rating (max %d): ", get_part_maximum_rating(PART));
                  break;
                case PART_STORAGE:
                  send_to_char(CH, "Storage capacity (max %d): ", get_part_maximum_rating(PART));
                  break;
                case PART_ACTIVE:
                  send_to_char(CH, "Memory capacity (max %d): ", get_part_maximum_rating(PART));
                  break;
                case PART_RESPONSE:
                  send_to_char(CH, "Response increase (max %d): ", get_part_maximum_rating(PART));
                  break;
                default:
                  send_to_char(CH, "Rating of part (max %d): ", get_part_maximum_rating(PART));
                  break;
              }

              d->edit_mode = DEDIT_RATING;
              break;
            }
            // Explicit fallthrough-- we only allow option 4 if the part can accept a rating in the first place.
            // fall through
        default:
            send_to_char(CH, "Invalid Option! Enter Option: ");
            break;
        }
        break;
    case DEDIT_RATING:
#define WARN_IF_PART_BELOW_RATING(min_rating, warning_string) if (number < (min_rating)) { send_to_char(CH, "^yWARNING:^n You've picked a pretty " #warning_string "! The maximum is %d. Be sure you want this!\r\n", get_part_maximum_rating(PART)); }
        switch (GET_PART_TYPE(PART)) {
          case PART_IO:
            if (number < 1 || number > get_part_maximum_rating(PART)) {
                send_to_char(CH, "I/O must be between 1 and %d. Enter Speed of I/O: ", get_part_maximum_rating(PART));
                return;
            }
            WARN_IF_PART_BELOW_RATING(GET_PART_TARGET_MPCP(PART) * 50, "slow I/O speed");
            break;
          case PART_STORAGE:
            if (number < 1 || number > get_part_maximum_rating(PART)) {
                send_to_char(CH, "Memory must be between 1 and %d. Enter Amount of Memory: ", get_part_maximum_rating(PART));
                return;
            }
            WARN_IF_PART_BELOW_RATING(GET_PART_TARGET_MPCP(PART) * 300, "low amount of storage memory");
            break;
          case PART_ACTIVE:
            if (number < 1 || number > get_part_maximum_rating(PART)) {
                send_to_char(CH, "Memory must be between 1 and %d. Enter Amount of Memory: ", get_part_maximum_rating(PART));
                return;
            }
            WARN_IF_PART_BELOW_RATING(GET_PART_TARGET_MPCP(PART) * 150, "low amount of active memory");
            break;
          case PART_RESPONSE:
            if (number < 0 || number > get_part_maximum_rating(PART)) {
                send_to_char(CH, "Response Increase must be between 1 and %d. Enter Response Increase: ", get_part_maximum_rating(PART));
                return;
            }
            WARN_IF_PART_BELOW_RATING(get_part_maximum_rating(PART) - 1, "low response rating");
            break;
          case PART_HARDENING:
          case PART_ICON:
          case PART_BOD:
          case PART_SENSOR:
          case PART_MASKING:
          case PART_EVASION:
          case PART_RADIO:
          case PART_CELLULAR:
          case PART_SATELLITE:
            if (number < 1 || number > get_part_maximum_rating(PART)) {
                send_to_char(CH, "Part ratings must be between 1 and the MPCP of the target deck (%d). Enter Rating: ", GET_PART_TARGET_MPCP(PART));
                return;
            }
            if (!part_is_nerps(GET_PART_TYPE(PART))) {
              WARN_IF_PART_BELOW_RATING(get_part_maximum_rating(PART) - 1, "low rating");
            }
            break;
        }
        GET_PART_RATING(PART) = number;
        partbuild_main_menu(d);
        break;
    case DEDIT_MPCP:
        if (number > GET_SKILL(CH, SKILL_BR_COMPUTER))
          send_to_char(CH, "You can't create a part for a deck with more MPCP than your Computer B/R Skill (%d). Enter Target MPCP: ", GET_SKILL(CH, SKILL_BR_COMPUTER));
        else if (number < 1 || number > MAX_CUSTOM_MPCP_RATING)
          send_to_char(CH, "Target MPCP must be between 1 and %d. Enter Target MPCP: ", MAX_CUSTOM_MPCP_RATING);
        else {
          if (GET_PART_TYPE(PART) == PART_MPCP) {
            GET_PART_RATING(PART) = number;
            GET_PART_TARGET_MPCP(PART) = number;
          } else {
            GET_PART_TARGET_MPCP(PART) = number;
            GET_PART_RATING(PART) = 0;
          }
          partbuild_main_menu(d);
        }
        break;
    case DEDIT_TYPE:
        if (number < PART_ACTIVE || number >= NUM_PARTS)
            send_to_char(CH, "Invalid Selection! Enter Part Number: ");
        else {
            GET_PART_TYPE(PART) = number;
            GET_PART_RATING(PART) = 0;
            partbuild_main_menu(d);
        }
        break;
    case DEDIT_NAME:
        int length_with_no_color = get_string_length_after_color_code_removal(arg, CH);

        // Silent failure: We already sent the error message in get_string_length_after_color_code_removal().
        if (length_with_no_color == -1) {
          partbuild_main_menu(d);
          return;
        }
        if (length_with_no_color >= LINE_LENGTH) {
            send_to_char(CH, "That name is too long, please shorten it. The maximum length after color code removal is %d characters.\r\n", LINE_LENGTH - 1);
            partbuild_main_menu(d);
            return;
        }

        if (strlen(arg) >= MAX_RESTRING_LENGTH) {
            send_to_char(CH, "That restring is too long, please shorten it. The maximum length with color codes included is %d characters.\r\n", MAX_RESTRING_LENGTH - 1);
            partbuild_main_menu(d);
            return;
        }

        DELETE_ARRAY_IF_EXTANT(PART->restring);
        PART->restring = str_dup(arg);
        partbuild_main_menu(d);
        break;
    }
}

void deckbuild_main_menu(struct descriptor_data *d) {
    CLS(CH);
    send_to_char(CH, "1) Name: ^c%s^n\r\n", GET_OBJ_NAME(PART));
    send_to_char(CH, "2) Description: ^c%s^n\r\n", PART->photo ? PART->photo : PART->text.look_desc);
    send_to_char(CH, "q) Quit\r\nEnter Option: ");
    d->edit_mode = DEDIT_MAIN;
}

void dbuild_parse(struct descriptor_data *d, const char *arg) {
    switch(d->edit_mode) {
    case DEDIT_MAIN:
        switch (*arg) {
        case '1':
            send_to_char(CH, "Enter cyberdeck name: ");
            d->edit_mode = DEDIT_NAME;
            break;
        case '2':
            send_to_char(CH, "Enter cyberdeck description:\r\n");
            d->edit_mode = DEDIT_TYPE;
            DELETE_D_STR_IF_EXTANT(d);
            INITIALIZE_NEW_D_STR(d);
            d->max_str = MAX_MESSAGE_LENGTH;
            d->mail_to = 0;
            break;
        case 'q':
        case 'Q':
            GET_OBJ_EXTRA(PART).SetBit(ITEM_EXTRA_KEPT);
            obj_to_char(PART, CH);
            PART = NULL;
            STATE(d) = CON_PLAYING;
            send_to_char(CH, "Saving deck.\r\n");
            break;
        }
        break;
    case DEDIT_NAME:
        int length_with_no_color = get_string_length_after_color_code_removal(arg, CH);

        // Silent failure: We already sent the error message in get_string_length_after_color_code_removal().
        if (length_with_no_color == -1) {
          deckbuild_main_menu(d);
          return;
        }
        if (length_with_no_color >= LINE_LENGTH) {
            send_to_char(CH, "That name is too long, please shorten it. The maximum length after color code removal is %d characters.\r\n", LINE_LENGTH - 1);
            deckbuild_main_menu(d);
            return;
        }

        if (strlen(arg) >= MAX_RESTRING_LENGTH) {
            send_to_char(CH, "That restring is too long, please shorten it. The maximum length with color codes included is %d characters.\r\n", MAX_RESTRING_LENGTH - 1);
            deckbuild_main_menu(d);
            return;
        }

        DELETE_ARRAY_IF_EXTANT(PART->restring);
        PART->restring = str_dup(arg);
        deckbuild_main_menu(d);
        break;
    }
}
void create_part(struct char_data *ch) {
    struct obj_data *part = read_object(OBJ_BLANK_PART_DESIGN, VIRTUAL);
    STATE(ch->desc) = CON_PART_CREATE;
    part->restring = str_dup("An empty part design");
    ch->desc->edit_obj = part;
    partbuild_main_menu(ch->desc);
}

void create_deck(struct char_data *ch) {
    struct obj_data *deck = read_object(OBJ_CUSTOM_CYBERDECK_SHELL, VIRTUAL);
    STATE(ch->desc) = CON_DECK_CREATE;
    ch->desc->edit_obj = deck;
    deckbuild_main_menu(ch->desc);

}

ACMD(do_cook) {
    struct obj_data *chip = NULL, *cooker;
    skip_spaces(&argument);
    if (!*argument) {
        send_to_char(ch, "What chip do you wish to burn?\r\n");
        return;
    }
    if (ch->in_veh) {
      if (ch->vfront) {
        send_to_char("There's not enough space up here-- try switching to the rear of the vehicle.\r\n", ch);
        return;
      } else if (!ch->in_veh->flags.IsSet(VFLAG_WORKSHOP)) {
        send_to_char("This vehicle doesn't have any power outlets for you to use.\r\n", ch);
        return;
      }
    }
    struct obj_data *comp;
    FOR_ITEMS_AROUND_CH(ch, comp) {
      if (GET_OBJ_TYPE(comp) == ITEM_DECK_ACCESSORY && GET_OBJ_VAL(comp, 0) == TYPE_COMPUTER && comp->contains)
        if ((chip = get_obj_in_list_vis(ch, argument, comp->contains)))
          break;
    }
    if (!chip)
        send_to_char(ch, "You don't see %s installed on any computers here.\r\n", argument);
    else if (GET_OBJ_TYPE(chip) != ITEM_PROGRAM)
        send_to_char(ch, "You must finish programming %s first.\r\n", GET_OBJ_NAME(chip));
    else if (GET_OBJ_TIMER(chip))
        send_to_char("This chip has already been encoded.\r\n", ch);
    else if (GET_OBJ_VAL(chip, 0) == SOFT_SUITE)
      send_to_char("Programming suites don't need to be cooked-- just leave them installed on the computer to get their benefits.\r\n", ch);
    else {
      FOR_ITEMS_AROUND_CH(ch, cooker) {
          if (GET_OBJ_TYPE(cooker) == ITEM_DECK_ACCESSORY && GET_OBJ_VAL(cooker, 0) == TYPE_COOKER && !cooker->contains)
              break;
      }
      if (!cooker) {
          send_to_char(ch, "There isn't a free chip encoder here.\r\n");
          return;
      }
      int cost = GET_OBJ_VAL(chip, 2) / 2, paid = 0;
      struct obj_data *obj;
      FOR_ITEMS_AROUND_CH(ch, obj) {
          if (GET_OBJ_TYPE(obj) == ITEM_DECK_ACCESSORY && GET_OBJ_VAL(obj, 0) == TYPE_PARTS && GET_OBJ_VAL(obj, 1) && GET_OBJ_COST(obj) >= cost) {
              GET_OBJ_COST(obj) -= cost;
              if (!GET_OBJ_COST(obj))
                  extract_obj(obj);
              paid = 1;
              break;
          }
      }
      if (!paid)
          for (struct obj_data *obj = ch->carrying;obj; obj = obj->next_content)
              if (GET_OBJ_TYPE(obj) == ITEM_DECK_ACCESSORY && GET_OBJ_VAL(obj, 0) == TYPE_PARTS && GET_OBJ_VAL(obj, 1) && GET_OBJ_COST(obj) >= cost) {
                  GET_OBJ_COST(obj) -= cost;
                  if (!GET_OBJ_COST(obj))
                      extract_obj(obj);
                  paid = 1;
                  break;
              }
      if (!paid) {
          send_to_char(ch, "You need at least %d nuyen worth of optical chips to encode %s.\r\n", cost, GET_OBJ_NAME(chip));
          return;
      }

      /* Instead of removing the software from the machine, we copy it instead if it's a cookable copyable thing. */
      if (program_can_be_copied(chip)) {
        struct obj_data *newp = read_object(OBJ_BLANK_PROGRAM, VIRTUAL);
        newp->restring = str_dup(GET_OBJ_NAME(chip));
        GET_OBJ_VAL(newp, 0) = GET_OBJ_VAL(chip, 0);
        GET_OBJ_VAL(newp, 1) = GET_OBJ_VAL(chip, 1);
        GET_OBJ_VAL(newp, 2) = GET_OBJ_VAL(chip, 2);
        GET_OBJ_VAL(newp, 3) = GET_OBJ_VAL(chip, 3);
        chip = newp;
        send_to_char("You save a copy to disk before sending it to your cooker.\r\n", ch);
      } else {
        // Can't be copied? OK, use the old behavior of removing the item.
        GET_OBJ_VAL(chip->in_obj, 3) -= GET_OBJ_VAL(chip, 2);
        obj_from_obj(chip);
        send_to_char(ch, "%s is too bespoke to be useful for a different deck, so you send it to your cooker without copying it first.\r\n", capitalize(GET_OBJ_NAME(chip)));
      }

      obj_to_obj(chip, cooker);

      int target = 4;
      int skill = get_skill(ch, SKILL_BR_COMPUTER, target) + MIN(GET_SKILL(ch, SKILL_BR_COMPUTER), GET_DECK_ACCESSORY_COOKER_RATING(cooker));
      int success = success_test(skill, target);
      if (success < 1) {
          success = srdice() + srdice();
          GET_OBJ_TIMER(chip) = -1;
      }
      GET_DECK_ACCESSORY_COOKER_TIME_REMAINING(cooker) = (GET_OBJ_VAL(chip, 1) * 24) / success;
      if (get_and_deduct_one_deckbuilding_token_from_char(ch)) {
        send_to_char("A deckbuilding token fuzzes into digital static, greatly accelerating the cooking time.\r\n", ch);
        GET_OBJ_TIMER(chip) = 0;
        GET_DECK_ACCESSORY_COOKER_TIME_REMAINING(cooker) = 1;
      }
      else if (access_level(ch, LVL_ADMIN)) {
        send_to_char("You use your admin powers to greatly accelerate the cooking time.\r\n", ch);
        GET_OBJ_TIMER(chip) = 0;
        GET_DECK_ACCESSORY_COOKER_TIME_REMAINING(cooker) = 1;
      }
      GET_DECK_ACCESSORY_COOKER_ORIGINAL_TIME(cooker) = GET_DECK_ACCESSORY_COOKER_TIME_REMAINING(cooker);
      act("The light on $p turns orange as it starts to cook the chip.", TRUE, ch, cooker, 0, TO_ROOM);
      act("The light on $p turns orange as it starts to cook the chip.", TRUE, ch, cooker, 0, TO_CHAR);
    }
}

void part_design(struct char_data *ch, struct obj_data *part) {

    if (!GET_OBJ_VAL(part, 3) || GET_OBJ_TYPE(part) != ITEM_PART)
        send_to_char(ch, "You don't need to design %s.\r\n", GET_OBJ_NAME(part));
    else if (GET_OBJ_VAL(part, 3) > 0) {
        if (GET_OBJ_VAL(part, 7) != GET_IDNUM(ch))
            send_to_char(ch, "Someone is already working on %s.\r\n", GET_OBJ_NAME(part));
        else {
            send_to_char(ch, "You continue to design %s.\r\n", GET_OBJ_NAME(part));
            AFF_FLAGS(ch).SetBit(AFF_PART_DESIGN);
            ch->char_specials.programming = part;
        }
    } else {
        int target = GET_PART_TARGET_MPCP(part)/2, skill = get_skill(ch, SKILL_CYBERTERM_DESIGN, target);
        GET_PART_DESIGN_COMPLETION(part) = GET_PART_TARGET_MPCP(part) * 2;
        GET_OBJ_VAL(part, 5) = success_test(skill, target) << 1;
        GET_PART_BUILDER_IDNUM(part) = GET_IDNUM(ch);
        if (get_and_deduct_one_deckbuilding_token_from_char(ch)) {
          send_to_char("A deckbuilding token fuzzes into digital static, greatly accelerating the design process.\r\n", ch);
          GET_PART_DESIGN_COMPLETION(part) = 1;
          GET_OBJ_VAL(part, 5) = 100;
        }
        if (access_level(ch, LVL_ADMIN)) {
          send_to_char("You use your admin powers to greatly accelerate the design process.\r\n", ch);
          GET_PART_DESIGN_COMPLETION(part) = 1;
          GET_OBJ_VAL(part, 5) = 100;
        }
        send_to_char(ch, "You begin to design %s.\r\n", GET_OBJ_NAME(part));
        AFF_FLAGS(ch).SetBit(AFF_PART_DESIGN);
        ch->char_specials.programming = part;
    }
}

ACMD(do_build) {
    char arg1[MAX_INPUT_LENGTH], arg2[MAX_INPUT_LENGTH], arg3[MAX_INPUT_LENGTH];
    struct obj_data *obj, *deck;
    if (!*argument) {
      if (IS_WORKING(ch)) {
        STOP_WORKING(ch);
        send_to_char("You stop working.\r\n", ch);
      } else
        send_to_char(ch, "What do you want to build?\r\n");
      return;
    }
    if (GET_POS(ch) > POS_SITTING) {
      do_sit(ch, NULL, 0, 0);
      if (GET_POS(ch) > POS_SITTING) {
        send_to_char(ch, "You have to be sitting to do that.\r\n");
        return;
      }
    }
    if (IS_WORKING(ch)) {
        send_to_char(TOOBUSY, ch);
        return;
    }
    half_chop(argument, arg1, buf);
    half_chop(buf, arg2, arg3);
    if (!(obj = get_obj_in_list_vis(ch, arg1, ch->carrying))) {
      if (ch->in_room)
        obj = get_obj_in_list_vis(ch, arg1, ch->in_room->contents);
      else
        obj = get_obj_in_list_vis(ch, arg1, ch->in_veh->contents);

      // They're building a new thing.
      if (!obj) {
        // Building a new circle or lodge?
        if (!str_cmp(arg1, "circle") || !str_cmp(arg1, "lodge")) {
          if (ch->in_veh) {
            send_to_char(ch, "You can't build circles or lodges in vehicles.\r\n");
            return;
          }

          FOR_ITEMS_AROUND_CH(ch, obj) {
            if (GET_OBJ_TYPE(obj) == ITEM_MAGIC_TOOL && (GET_MAGIC_TOOL_TYPE(obj) == TYPE_CIRCLE || GET_MAGIC_TOOL_TYPE(obj) == TYPE_LODGE)) {
              send_to_char("There is already a lodge or a hermetic circle here.\r\n", ch);
              return;
            }
          }

          // Circle-specific code.
          if (!str_cmp(arg1, "circle")) {
            if (!*arg2 || !*arg3)
              send_to_char("Draw the circle at what force and to what element?\r\n", ch);
            else
              circle_build(ch, arg3, atoi(arg2));
            return;
          }

          // Lodge-specific code.
          if (!str_cmp(arg1, "lodge")) {
            if (!*arg2)
              send_to_char("What force do you wish that lodge to be?\r\n", ch);
            else
              lodge_build(ch, atoi(arg2));
            return;
          }
        }

        // Whatever they're building isn't supported, so they must have typod a part.
        send_to_char(ch, "You don't seem to have that part.\r\n");
        return;
      }

      // We know obj exists.
      if (GET_OBJ_TYPE(obj) == ITEM_MAGIC_TOOL) {
          if (!GET_OBJ_VAL(obj, 9))
              send_to_char("It has already been completed!\r\n", ch);
          else if (GET_OBJ_VAL(obj, 3) != GET_IDNUM(ch))
              send_to_char("That's not yours!\r\n", ch);
          else if (GET_MAGIC_TOOL_TYPE(obj) == TYPE_LODGE) {
              AFF_FLAGS(ch).SetBit(AFF_LODGE);
              GET_BUILDING(ch) = obj;
              send_to_char(ch, "You continue working on your lodge.\r\n");
              act("$n continues to build $s lodge.", FALSE, ch, 0, 0, TO_ROOM);
          } else if (GET_MAGIC_TOOL_TYPE(obj) == TYPE_CIRCLE) {
              AFF_FLAGS(ch).SetBit(AFF_CIRCLE);
              GET_BUILDING(ch) = obj;
              send_to_char(ch, "You continue working on %s.\r\n", GET_OBJ_NAME(obj));
              act("$n continues to draw the hermetic circle.", FALSE, ch, 0, 0, TO_ROOM);
          } else
              send_to_char(ch, "There's nothing about %s that strikes you as buildable.\r\n", GET_OBJ_NAME(obj));
      } else
          send_to_char(ch, "There's nothing about %s that strikes you as buildable.\r\n", GET_OBJ_NAME(obj));
      return;

    } else if (GET_OBJ_TYPE(obj) == ITEM_GUN_AMMO) {
      ammo_build(ch, obj);
      return;
    }

    if (!*arg2) {
      send_to_char("Syntax: BUILD <part> <deck>\r\n", ch);
      return;
    }

    if (!(deck = get_obj_in_list_vis(ch, arg2, ch->carrying))) {
      send_to_char(ch, "You don't have a deck named '%s'.\r\n", arg2);
      return;
    }

    if(GET_OBJ_TYPE(obj) != ITEM_PART) {
      send_to_char(ch, "You can't build a cyberdeck part out of %s.\r\n", decapitalize_a_an(GET_OBJ_NAME(obj)));
      return;
    }

    if (GET_OBJ_TYPE(deck) != ITEM_CUSTOM_DECK) {
      send_to_char(ch, "%s isn't a custom deck, so you can't build anything into it.\r\n", capitalize(GET_OBJ_NAME(deck)));
      return;
    }

    if (GET_PART_DESIGN_COMPLETION(obj)) {
      send_to_char(ch, "You must make a design for %s first.\r\n", decapitalize_a_an(GET_OBJ_NAME(obj)));
      return;
    }

    if (GET_PART_BUILDER_IDNUM(obj) != GET_IDNUM(ch) && GET_PART_BUILDER_IDNUM(obj) > 0) {
      send_to_char(ch, "Someone else has already started on %s.\r\n", decapitalize_a_an(GET_OBJ_NAME(obj)));
      return;
    }

    if (GET_PART_TYPE(obj) == PART_MPCP && GET_CYBERDECK_MPCP(deck)) {
        struct obj_data *temp = deck->contains;
        if (GET_PART_TARGET_MPCP(obj) != GET_CYBERDECK_MPCP(deck))
            for (; temp; temp = temp->next_content)
                if (GET_OBJ_TYPE(temp) == ITEM_PART && GET_PART_TARGET_MPCP(obj) == GET_PART_TARGET_MPCP(temp))
                    break;
        if (!temp) {
            send_to_char(ch, "%s is not designed for the same MPCP as %s.\r\n", capitalize(GET_OBJ_NAME(obj)), decapitalize_a_an(GET_OBJ_NAME(deck)));
            return;
        }
    }

    if (GET_PART_TYPE(obj) != PART_MPCP
             && (GET_CYBERDECK_MPCP(deck) && GET_PART_TARGET_MPCP(obj) != GET_CYBERDECK_MPCP(deck))
             && (parts[GET_PART_TYPE(obj)].design >= 0
                 || GET_PART_TYPE(obj) == PART_ACTIVE
                 || GET_PART_TYPE(obj) == PART_STORAGE
                 || GET_PART_TYPE(obj) == PART_MATRIX_INTERFACE))
        send_to_char(ch, "%s is not designed for the same MPCP as %s.\r\n", capitalize(GET_OBJ_NAME(obj)), decapitalize_a_an(GET_OBJ_NAME(deck)));
    else {
        struct obj_data *workshop = NULL;
        int kit_rating = 0, target = -GET_OBJ_VAL(obj, 5), duration = 0, skill = 0;
        if (has_kit(ch, TYPE_MICROTRONIC))
            kit_rating = TYPE_KIT;
        if ((workshop = find_workshop(ch, TYPE_MICROTRONIC)) && GET_OBJ_VAL(workshop, 0) > kit_rating)
            kit_rating = GET_WORKSHOP_TYPE(workshop);
        if (kit_rating < parts[GET_PART_TYPE(obj)].tools) {
            send_to_char(ch, "You don't have the right tools for that job. You need to %s %s %s.\r\n",
                         parts[GET_PART_TYPE(obj)].tools == TYPE_WORKSHOP ? "have an unpacked" : "have a",
                         workshops[TYPE_MICROTRONIC],
                         kit_workshop_facility[parts[GET_PART_TYPE(obj)].tools]);
            return;
        } else if (parts[GET_OBJ_VAL(obj, 0)].tools == TYPE_KIT) {
            if (kit_rating == TYPE_WORKSHOP)
                target--;
            else if (kit_rating == TYPE_FACILITY)
                target -= 3;
        } else if (parts[GET_OBJ_VAL(obj, 0)].tools == TYPE_WORKSHOP && kit_rating == TYPE_FACILITY)
            target--;
        if (GET_PART_TYPE(obj) == PART_BOD || GET_PART_TYPE(obj) == PART_SENSOR || GET_PART_TYPE(obj) == PART_MASKING || GET_PART_TYPE(obj) == PART_EVASION) {
            int total = GET_PART_RATING(obj);
            for (struct obj_data *part = deck->contains; part; part = part->next_content)
                if (GET_OBJ_TYPE(part) == ITEM_PART && (GET_PART_TYPE(part) == PART_BOD || GET_PART_TYPE(part) == PART_SENSOR || GET_PART_TYPE(part) == PART_MASKING || GET_PART_TYPE(part) == PART_EVASION))
                    total += GET_OBJ_VAL(part, 1);
            if (total > GET_CYBERDECK_MPCP(deck) * 3) {
                send_to_char(ch, "The total persona program rating for MPCP %d decks is limited to %d; that would make it %d.\r\n",
                             GET_CYBERDECK_MPCP(deck),
                             GET_CYBERDECK_MPCP(deck) * 3,
                             total);
                return;
            }
        }
        for (struct obj_data *part = deck->contains; part; part = part->next_content)
            if (GET_OBJ_TYPE(part) == TYPE_PARTS && (GET_PART_TYPE(part) == GET_PART_TYPE(obj) || (GET_PART_TYPE(part) ==
                    PART_ASIST_HOT && GET_PART_TYPE(obj) == PART_ASIST_COLD) || (GET_PART_TYPE(part) == PART_ASIST_COLD && GET_PART_TYPE(obj) == PART_ASIST_HOT))) {
                send_to_char(ch, "You have already installed a part of that type in there.\r\n");
                return;
            }
        if (GET_OBJ_VAL(obj, 4) > 0) {
            send_to_char(ch, "You continue work on building %s.\r\n", GET_OBJ_NAME(obj));
            obj->contains = deck;
            ch->char_specials.programming = obj;
            AFF_FLAGS(ch).SetBit(AFF_PART_BUILD);
        } else {
            for (struct obj_data *part = deck->contains; part; part = part->next_content)
                if (GET_OBJ_TYPE(part) == TYPE_PARTS && (GET_OBJ_VAL(part, 0) == GET_OBJ_VAL(obj, 0) || (GET_OBJ_VAL(part, 0) ==
                        PART_ASIST_HOT && GET_OBJ_VAL(obj, 0) == PART_ASIST_COLD) || (GET_OBJ_VAL(part, 0) == PART_ASIST_COLD && GET_OBJ_VAL(obj, 0) == PART_ASIST_HOT))) {
                    send_to_char(ch, "You have already installed a part of that type in there.\r\n");
                    return;
                }
            if (GET_OBJ_VAL(obj, 4) == -1) {
                struct obj_data *soft = NULL;
                if (parts[GET_OBJ_VAL(obj, 0)].software) {
                    for (soft = ch->carrying; soft; soft = soft->next_content)
                        if (GET_OBJ_VAL(soft, 0) == parts[GET_OBJ_VAL(obj, 0)].software
                            && GET_OBJ_TIMER(soft) == 1
                            && GET_OBJ_VAL(soft, 1) == GET_OBJ_VAL(obj, 1))
                            break;
                    if (!soft) {
                        send_to_char(ch,
                                     "You need to program and cook the required software (rating %d %s) for that first.\r\n",
                                     GET_OBJ_VAL(obj, 1),
                                     programs[parts[GET_OBJ_VAL(obj, 0)].software].name);
                        return;
                    }
                }
                struct obj_data *chips = NULL, *part = NULL, *find;
                FOR_ITEMS_AROUND_CH(ch, find) {
                    if (GET_OBJ_TYPE(find) == ITEM_DECK_ACCESSORY && GET_DECK_ACCESSORY_TYPE(find) == TYPE_PARTS) {
                        if (GET_DECK_ACCESSORY_IS_CHIPS(find) && GET_OBJ_COST(find) >= GET_OBJ_VAL(obj, 9))
                            chips = find;
                        else if (!GET_DECK_ACCESSORY_IS_CHIPS(find) && GET_OBJ_COST(find) >= GET_OBJ_VAL(obj, 8))
                            part = find;
                    }
                }
                for (struct obj_data *find = ch->carrying; find; find = find->next_content)
                    if (GET_OBJ_TYPE(find) == ITEM_DECK_ACCESSORY && GET_DECK_ACCESSORY_TYPE(find) == TYPE_PARTS) {
                        if (GET_DECK_ACCESSORY_IS_CHIPS(find) && GET_OBJ_COST(find) >= GET_OBJ_VAL(obj, 9))
                            chips = find;
                        else if (!GET_DECK_ACCESSORY_IS_CHIPS(find) && GET_OBJ_COST(find) >= GET_OBJ_VAL(obj, 8))
                            part = find;
                    }

                if (GET_OBJ_VAL(obj, 8) && !part) {
                  send_to_char(ch, "You don't have enough materials for that part; you need %d nuyen's worth in the same container.\r\n", GET_OBJ_VAL(obj, 8));
                  return;
                }
                if (GET_OBJ_VAL(obj, 9) && !chips) {
                  send_to_char(ch, "You don't have enough optical chips for that part; you need %d nuyen's worth in the same container.\r\n", GET_OBJ_VAL(obj, 9));
                  return;
                }

                if (GET_OBJ_VAL(obj, 8)) {
                  GET_OBJ_COST(part) -= GET_OBJ_VAL(obj, 8);
                  if (!GET_OBJ_COST(part)) {
                    send_to_char(ch, "You use up the last of the parts in %s.\r\n", GET_OBJ_NAME(part));
                    extract_obj(part);
                  }
                }

                if (GET_OBJ_VAL(obj, 9)) {
                  GET_OBJ_COST(chips) -= GET_OBJ_VAL(obj, 9);
                  if (!GET_OBJ_COST(chips)) {
                    send_to_char(ch, "You use up the last of the chips in %s.\r\n", GET_OBJ_NAME(chips));
                    extract_obj(chips);
                  }
                }


                if (soft)
                    extract_obj(soft);
            }
            switch (GET_OBJ_VAL(obj, 0)) {
            case PART_ACTIVE:
                target += 4;
                duration = GET_OBJ_VAL(obj, 1) / 200;
                break;
            case PART_ASIST_HOT:
            case PART_ASIST_COLD:
                target += GET_OBJ_VAL(obj, 2);
                duration = GET_OBJ_VAL(obj, 2) * 24;
                break;
            case PART_HARDENING:
                target += GET_OBJ_VAL(obj, 2);
                duration = GET_OBJ_VAL(obj, 2) * GET_OBJ_VAL(obj, 1) * 24;
                break;
            case PART_ICCM:
                target += 4;
                duration = GET_OBJ_VAL(obj, 2) * 48;
                break;
            case PART_ICON:
                target += GET_OBJ_VAL(obj, 1);
                duration = 1;
                break;
            case PART_IO:
                target += (int)(GET_OBJ_VAL(obj, 1) / 100);
                duration = (int)(GET_OBJ_VAL(obj, 1) / 100) * 24;
                break;
            case PART_MASER:
                target += GET_OBJ_VAL(obj, 2);
                duration = GET_OBJ_VAL(obj, 2) + 4;
                break;
            case PART_MATRIX_INTERFACE:
            case PART_PORTS:
            case PART_SIGNAL_AMP:
                target += 4;
                duration = 1;
                break;
            case PART_MPCP:
                target += GET_OBJ_VAL(obj, 1);
                duration = GET_OBJ_VAL(obj, 1);
                break;
            case PART_BOD:
            case PART_SENSOR:
            case PART_MASKING:
            case PART_EVASION:
                target += GET_OBJ_VAL(obj, 1);
                duration = 1;
                break;
            case PART_RAS_OVERRIDE:
                target += GET_OBJ_VAL(obj, 2);
                duration = GET_OBJ_VAL(obj, 2) * 24;
                break;
            case PART_RESPONSE:
                target += GET_OBJ_VAL(obj, 1) * 2;
                duration = GET_OBJ_VAL(obj, 1) + GET_OBJ_VAL(obj, 2);
                break;
            case PART_STORAGE:
                target += 4;
                duration = GET_OBJ_VAL(obj, 1) / 200;
                break;
            case PART_CELLULAR:
            case PART_RADIO:
            case PART_MICROWAVE:
                target += GET_OBJ_VAL(obj, 2);
                duration = GET_OBJ_VAL(obj, 1) + GET_OBJ_VAL(obj, 2);
                break;
            case PART_LASER:
            case PART_SATELLITE:
                target += GET_OBJ_VAL(obj, 2);
                duration = GET_OBJ_VAL(obj, 2) + 4;
                break;
            }
            duration *= 60;
            skill = get_skill(ch, SKILL_BR_COMPUTER, target);
            int success = success_test(skill, target);
            target = 0;

            switch(GET_OBJ_VAL(obj, 0)) {
            case PART_ICCM:
            case PART_REALITY_FILTER:
                target = GET_OBJ_VAL(obj, 2);
                skill = SKILL_COMPUTER;
                break;
            case PART_MASER:
            case PART_PORTS:
            case PART_LASER:
            case PART_MICROWAVE:
                target = 4;
                skill = SKILL_BR_ELECTRONICS;
                break;
            case PART_RADIO:
            case PART_SATELLITE:
            case PART_SIGNAL_AMP:
            case PART_CELLULAR:
                target = GET_OBJ_VAL(obj, 1);
                skill = SKILL_BR_ELECTRONICS;
                break;
            }
            if (target) {
                skill = get_skill(ch, skill, target);
                if (success_test(skill, target) < 0)
                    success = -1;
            }
            if (success < 1) {
                success = srdice() + srdice();
                GET_OBJ_TIMER(obj) = -1;
            }
            GET_OBJ_VAL(obj, 10) = GET_OBJ_VAL(obj, 4) = duration / success;

            if (get_and_deduct_one_deckbuilding_token_from_char(ch)) {
              send_to_char("A deckbuilding token fuzzes into digital static, greatly accelerating the build time.\r\n", ch);
              GET_OBJ_VAL(obj, 10) = GET_OBJ_VAL(obj, 4) = 1;
              GET_OBJ_TIMER(obj) = 0;
            }
            else if (access_level(ch, LVL_ADMIN)) {
              send_to_char("You use your admin powers to greatly accelerate the build time.\r\n", ch);
              GET_OBJ_VAL(obj, 10) = GET_OBJ_VAL(obj, 4) = 1;
              GET_OBJ_TIMER(obj) = 0;
            }
            send_to_char(ch, "You start building %s.\r\n", GET_OBJ_NAME(obj));
            ch->char_specials.programming = obj;
            obj->contains = deck;
            AFF_FLAGS(ch).SetBit(AFF_PART_BUILD);
            if (!GET_OBJ_VAL(deck, 0))
                GET_OBJ_VAL(deck, 0) = GET_OBJ_VAL(obj, 2);
        }
    }
}


ACMD(do_progress)
{
  int amount_left, amount_needed;

  if (AFF_FLAGS(ch).IsSet(AFF_LODGE) || AFF_FLAGS(ch).IsSet(AFF_CIRCLE)) {
    int initial_build_time = MAX(1, GET_MAGIC_TOOL_RATING(GET_BUILDING(ch)) * (AFF_FLAGS(ch).IsSet(AFF_CIRCLE) ? 60 : 300));
    float completion_percentage = ((float)(initial_build_time - GET_MAGIC_TOOL_BUILD_TIME_LEFT(GET_BUILDING(ch))) / initial_build_time) * 100;

    if (GET_MAGIC_TOOL_BUILD_TIME_LEFT(GET_BUILDING(ch)) && GET_MAGIC_TOOL_OWNER(GET_BUILDING(ch)) == GET_IDNUM(ch)) {
      send_to_char(ch, "It looks like you've completed around %.02f%% of your %s.\r\n", completion_percentage, AFF_FLAGS(ch).IsSet(AFF_CIRCLE) ? "hermetic circle" : "lodge");
    }
    return;
  }

  if (AFF_FLAGS(ch).IsSet(AFF_PACKING)) {
    struct obj_data *obj;
    FOR_ITEMS_AROUND_CH(ch, obj) {
      if (GET_OBJ_TYPE(obj) == ITEM_WORKSHOP && GET_OBJ_VAL(obj, 3)) {
        send_to_char(ch, "You are about %d%% of the way through%spacking %s.\r\n",
                          (int)((float)((3.0-GET_OBJ_VAL(obj, 3)) / 3)*100), GET_OBJ_VAL(obj, 2) ? " " : " un",
                         GET_OBJ_NAME(obj));
        break;
      }
    }
    return;
  }

  if (AFF_FLAGS(ch).IsSet(AFF_RITUALCAST)) {
    amount_left   = GET_RITUAL_TICKS_LEFT(GET_BUILDING(ch));
    amount_needed = GET_RITUAL_TICKS_AT_START(GET_BUILDING(ch));
    send_to_char(ch, "You are about %2.2f%% of the way through casting %s.\r\n",
           (((float)(amount_needed - amount_left) * 100) / amount_needed), GET_OBJ_NAME(GET_BUILDING(ch)));
    return;
  }

  if (AFF_FLAGS(ch).IsSet(AFF_BONDING)) {
    if (GET_OBJ_TYPE(GET_BUILDING(ch)) == ITEM_WEAPON) {
      int initial_bond_time = MAX(1, GET_WEAPON_FOCUS_RATING(GET_BUILDING(ch)) * 60);
      float completion_percentage = ((float)(initial_bond_time - GET_WEAPON_FOCUS_BOND_STATUS(GET_BUILDING(ch))) / initial_bond_time) * 100;

      send_to_char(ch, "You are about %.02f%% of the way through bonding %s.\r\n", completion_percentage, GET_OBJ_NAME(GET_BUILDING(ch)));
    } else {
      send_to_char(ch, "There are %d ticks remaining until you finish bonding %s.\r\n", GET_OBJ_VAL(GET_BUILDING(ch), 9), GET_OBJ_NAME(GET_BUILDING(ch)));
    }
    return;
  }

  if (AFF_FLAGS(ch).IsSet(AFF_PROGRAM)) {
    amount_left   = GET_OBJ_VAL(GET_BUILDING(ch), 5);
    amount_needed = GET_OBJ_TIMER(GET_BUILDING(ch));
    send_to_char(ch, "You are about %2.2f%% of the way through programming %s.\r\n",
           (((float)(amount_needed - amount_left) * 100) / amount_needed), GET_OBJ_NAME(GET_BUILDING(ch)));
    return;
  }

  if (AFF_FLAGS(ch).IsSet(AFF_PART_BUILD)) {
    amount_left   = GET_OBJ_VAL(GET_BUILDING(ch), 4);
    amount_needed = GET_OBJ_VAL(GET_BUILDING(ch), 10);
    send_to_char(ch, "You are about %2.2f%% of the way through building %s.\r\n",
           (((float)(amount_needed - amount_left) * 100) / amount_needed), GET_OBJ_NAME(GET_BUILDING(ch)));
    return;
  }

  if (AFF_FLAGS(ch).IsSet(AFF_PART_DESIGN)) {
    amount_left = GET_PART_DESIGN_COMPLETION(GET_BUILDING(ch));
    amount_needed = GET_PART_TARGET_MPCP(GET_BUILDING(ch)) * 2;
    send_to_char(ch, "You are about %2.2f%% of the way through designing %s.\r\n",
           (((float)(amount_needed - amount_left) * 100) / amount_needed), GET_OBJ_NAME(GET_BUILDING(ch)));
    return;
  }

  if (AFF_FLAGS(ch).IsSet(AFF_DESIGN)) {
    amount_left = GET_OBJ_VAL(GET_BUILDING(ch), 4);
    amount_needed = GET_OBJ_TIMER(GET_BUILDING(ch));
    send_to_char(ch, "You are about %2.2f%% of the way through designing %s.\r\n",
           (((float)(amount_needed - amount_left) * 100) / amount_needed), GET_OBJ_NAME(GET_BUILDING(ch)));
    return;
  }

  if (AFF_FLAGS(ch).IsSet(AFF_CONJURE)) {
    float current = ch->char_specials.conjure[3] - ch->char_specials.conjure[2];
    float target = ch->char_specials.conjure[3];
    float percentage = (current / target) * 100;

    send_to_char(ch, "You are about %d%% of the way through the conjuring process.\r\n", (int) percentage);
    return;
  }

  if (AFF_FLAGS(ch).IsSet(AFF_SPELLDESIGN)) {
    int timeleft = GET_SPELLFORMULA_TIME_LEFT(ch->char_specials.programming);
    if (GET_OBJ_TIMER(ch->char_specials.programming) == SPELL_DESIGN_FAILED_CODE)
      timeleft *= 2;
    send_to_char(ch, "You are about %d%% done designing %s.\r\n", (int)(((float)timeleft / (float)GET_SPELLFORMULA_INITIAL_TIME(ch->char_specials.programming)) *-100 + 100), GET_OBJ_NAME(ch->char_specials.programming));
    return;
  }

  if (AFF_FLAGS(ch).IsSet(AFF_AMMOBUILD)) {
    send_to_char(ch, "You are about %d%% of the way through making a batch of ammo for %s.\r\n",
           (int)(((float)(GET_OBJ_VAL(GET_BUILDING(ch), 10) - GET_OBJ_VAL(GET_BUILDING(ch), 4)) /
           GET_OBJ_VAL(GET_BUILDING(ch), 10)) * 100), GET_OBJ_NAME(GET_BUILDING(ch)));
    return;
  } else
    send_to_char("You are not working on anything at this time.\r\n", ch);
}
