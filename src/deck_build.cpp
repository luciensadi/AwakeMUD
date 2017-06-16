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
#include <stdlib.h>

#define CH d->character
#define PART d->edit_obj
#define DEDIT_MAIN 0
#define DEDIT_TYPE 1
#define DEDIT_MPCP 2
#define DEDIT_NAME 3
#define DEDIT_RATING 4

extern void ammo_build(struct char_data *ch, struct obj_data *obj);

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
    case PART_MPCP:
    case PART_BOD:
    case PART_SENSOR:
    case PART_MASKING:
    case PART_EVASION:
    case PART_CELLULAR:
    case PART_RADIO:
    case PART_SATELLITE:
        return 35 * (rating * rating);
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
        return mpcpcost;
    case PART_ICON:
    case PART_HARDENING:
    case PART_MPCP:
        return ratingcost;
    case PART_RESPONSE:
        return (mpcp*mpcp) * (rating*2);
    }
    return 0;
}

void partbuild_main_menu(struct descriptor_data *d) {
    CLS(CH);
    send_to_char(CH, "1) Name: ^c%-40s^n     Software Needed: ^c%s^n\r\n",
                 PART->restring, YESNO(parts[GET_OBJ_VAL(PART, 0)].software));
    send_to_char(CH, "2) Type: ^c%-40s^n     Parts Cost: ^c%d\xC2\xA5^n\r\n",
                 parts[GET_OBJ_VAL(PART, 0)].name, get_part_cost(GET_OBJ_VAL(PART, 0), GET_OBJ_VAL(PART, 1), GET_OBJ_VAL(PART, 2)));
    send_to_char(CH, "3) MPCP Designed For: ^c%-30d^n  Chips Cost: ^c%d\xC2\xA5^n\r\n",
                 GET_OBJ_VAL(PART, 2),
                 get_chip_cost(GET_OBJ_VAL(PART, 0), GET_OBJ_VAL(PART, 1), GET_OBJ_VAL(PART, 2)));
    if (GET_OBJ_VAL(PART, 2))
        switch (GET_OBJ_VAL(PART, 0)) {
        case PART_RESPONSE:
            if (GET_OBJ_VAL(PART, 2) < 3)
                GET_OBJ_VAL(PART, 2) = 3;
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
            send_to_char(CH, "4) Rating: ^c%d^n\r\n", GET_OBJ_VAL(PART, 1));
        }
    send_to_char(CH, "q) Save and Quit\r\n");
    send_to_char(CH, "Enter Option: ");
    d->edit_mode = DEDIT_MAIN;
}

void partbuild_disp_types(struct descriptor_data *d) {
    CLS(CH);
    for (int x = 1; x < NUM_PARTS; x += 2)
        send_to_char(CH, "%2d) %-28s %2d) %s\r\n", x, parts[x].name, x+1, x+1 < NUM_PARTS ? parts[x+1].name : " ");
    send_to_char(CH, "Enter Part Number: ");
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
            if (parts[GET_OBJ_VAL(PART, 0)].design >= 0 ||
                    GET_OBJ_VAL(PART, 0) == PART_ACTIVE || GET_OBJ_VAL(PART, 0) == PART_STORAGE || GET_OBJ_VAL(PART, 0) == PART_MATRIX_INTERFACE) {
                send_to_char(CH, "MPCP of Target Deck: ");
                d->edit_mode = DEDIT_MPCP;
            } else
                send_to_char(CH, "That is not needed for this part. Enter Option: ");
            break;
        case 'q':
        case 'Q':
            GET_OBJ_VAL(PART, 9) = get_chip_cost(GET_OBJ_VAL(PART, 0), GET_OBJ_VAL(PART, 1), GET_OBJ_VAL(PART, 2));
            GET_OBJ_VAL(PART, 8) = get_part_cost(GET_OBJ_VAL(PART, 0), GET_OBJ_VAL(PART, 1), GET_OBJ_VAL(PART, 2));
            if (parts[GET_OBJ_VAL(PART, 0)].design == -1)
                GET_OBJ_VAL(PART, 3) = 0;
            else
                GET_OBJ_VAL(PART, 3) = -1;
            GET_OBJ_VAL(PART, 4) = -1;
            if (!GET_OBJ_VAL(PART, 1))
                GET_OBJ_VAL(PART, 1) = GET_OBJ_VAL(PART, 2);
            obj_to_char(PART, CH);
            PART = NULL;
            STATE(d) = CON_PLAYING;
            send_to_char(CH, "Design Saved!\r\n");
            break;
        case '4':
            if (GET_OBJ_VAL(PART, 2)) {
                send_to_char(CH, "Rating of Part: ");
                d->edit_mode = DEDIT_RATING;
                break;
            }
        default:
            send_to_char(CH, "Invalid Option! Enter Option: ");
            break;
        }
        break;
    case DEDIT_RATING:
        switch (GET_OBJ_VAL(PART, 0)) {
        case PART_IO:
            if (number < 1 || number > GET_OBJ_VAL(PART, 2) * 100) {
                send_to_char(CH, "I/O must be between 1 and %d. Enter Speed of I/O: ", GET_OBJ_VAL(PART, 2) * 100);
                return;
            }
            break;
        case PART_STORAGE:
            if (number < 1 || number > GET_OBJ_VAL(PART, 2) * 600) {
                send_to_char(CH, "Memory must be between 1 and %d. Enter Amount of Memory: ", GET_OBJ_VAL(PART, 2) * 600);
                return;
            }
            break;
        case PART_ACTIVE:
            if (number < 1 || number > GET_OBJ_VAL(PART, 2) * 250) {
                send_to_char(CH, "Memory must be between 1 and %d. Enter Amount of Memory: ", GET_OBJ_VAL(PART, 2) * 250);
                return;
            }
            break;
        case PART_RESPONSE:
            if (number < 0 || number > MIN(3, (int)(GET_OBJ_VAL(PART, 2) / 4))) {
                send_to_char(CH, "Response Increase must be between 1 and %d. Enter Response Increase: ",
                             MIN(3, (int)(GET_OBJ_VAL(PART, 2) / 4)));
                return;
            }
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
            if (number < 1 || number > GET_OBJ_VAL(PART, 2)) {
                send_to_char(CH, "Part ratings cannot exceed MPCP of the target deck. Enter Rating: ");
                return;
            }
            break;
        }
        GET_OBJ_VAL(PART, 1) = number;
        partbuild_main_menu(d);
        break;
    case DEDIT_MPCP:
        if (number > GET_SKILL(CH, SKILL_BR_COMPUTER))
            send_to_char(CH, "You can't create a part for a deck with more MPCP than your Computer B/R Skill. Enter Target MPCP: ");
        else if (number < 1 || number > 15)
            send_to_char(CH, "Target MPCP must be between 1 and 15. Enter Target MPCP: ");
        else {
            if (GET_OBJ_VAL(PART, 0) == PART_MPCP)
                GET_OBJ_VAL(PART, 1) = GET_OBJ_VAL(PART, 2) = number;
            else {
                GET_OBJ_VAL(PART, 2) = number;
                GET_OBJ_VAL(PART, 1) = 0;
            }
            partbuild_main_menu(d);
        }
        break;
    case DEDIT_TYPE:
        if (number < 1 || number > 25)
            send_to_char(CH, "Invalid Selection! Enter Part Number: ");
        else {
            GET_OBJ_VAL(PART, 0) = number;
            GET_OBJ_VAL(PART, 1) = 0;
            partbuild_main_menu(d);
        }
        break;
    case DEDIT_NAME:
        if (strlen(arg) >= LINE_LENGTH) {
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
            CLEANUP_AND_INITIALIZE_D_STR(d);
            d->max_str = MAX_MESSAGE_LENGTH;
            d->mail_to = 0;
            break;
        case 'q':
        case 'Q':
            obj_to_char(PART, CH);
            PART = NULL;
            STATE(d) = CON_PLAYING;
            send_to_char(CH, "Saving deck.\r\n");
            break;
        }
        break;
    case DEDIT_NAME:
        if (strlen(arg) >= LINE_LENGTH) {
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
    struct obj_data *part = read_object(112, VIRTUAL);
    STATE(ch->desc) = CON_PART_CREATE;
    part->restring = str_dup("An empty part design");
    ch->desc->edit_obj = part;
    partbuild_main_menu(ch->desc);
}

void create_deck(struct char_data *ch) {
    struct obj_data *deck = read_object(113, VIRTUAL);
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
        send_to_char("You must be in the rear of a vehicle to do that.\r\n", ch);
        return;
      } else if (!ch->in_veh->flags.IsSet(VFLAG_WORKSHOP)) {
        send_to_char("This vehicle doesn't have any power outlets for you to use.\r\n", ch);
        return;
      }
    }
    for (struct obj_data *comp = ch->in_veh ? ch->in_veh->contents : world[ch->in_room].contents; comp; comp = comp->next_content)
        if (GET_OBJ_TYPE(comp) == ITEM_DECK_ACCESSORY && GET_OBJ_VAL(comp, 0) == TYPE_COMPUTER && comp->contains)
            if ((chip = get_obj_in_list_vis(ch, argument, comp->contains)))
                break;
    if (!chip)
        send_to_char(ch, "You need to install that chip on a computer.\r\n");
    else if (GET_OBJ_TYPE(chip) != ITEM_PROGRAM)
        send_to_char(ch, "You must finish programming it first.\r\n");
    else if (GET_OBJ_TIMER(chip))
        send_to_char("This chip has already been encoded.\r\n", ch);
    else if (GET_OBJ_VAL(chip, 0) == SOFT_SUITE)
      send_to_char("You don't need to encode this to an optical chip.\r\n", ch);
    else {
        for (cooker = ch->in_veh ? ch->in_veh->contents : world[ch->in_room].contents; cooker; cooker = cooker->next_content)
            if (GET_OBJ_TYPE(cooker) == ITEM_DECK_ACCESSORY && GET_OBJ_VAL(cooker, 0) == TYPE_COOKER && !cooker->contains)
                break;
        if (!cooker) {
            send_to_char(ch, "There isn't a free chip encoder here.\r\n");
            return;
        }
        int cost = GET_OBJ_VAL(chip, 2) / 2, paid = 0;
        for (struct obj_data *obj = ch->in_veh ? ch->in_veh->contents : world[ch->in_room].contents; obj; obj = obj->next_content)
            if (GET_OBJ_TYPE(obj) == ITEM_DECK_ACCESSORY && GET_OBJ_VAL(obj, 0) == TYPE_PARTS && GET_OBJ_VAL(obj, 1) && GET_OBJ_COST(obj) >= cost) {
                GET_OBJ_COST(obj) -= cost;
                if (!GET_OBJ_COST(obj))
                    extract_obj(obj);
                paid = 1;
                break;
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
            send_to_char(ch, "You don't have a large enough optical chip to encode that onto.\r\n");
            return;
        }
        GET_OBJ_VAL(chip->in_obj, 3) -= GET_OBJ_VAL(chip, 2);
        obj_from_obj(chip);
        obj_to_obj(chip, cooker);
        int target = 4;
        int skill = get_skill(ch, SKILL_BR_COMPUTER, target) + MIN(GET_SKILL(ch, SKILL_BR_COMPUTER), GET_OBJ_VAL(cooker, 0));
        int success = success_test(skill, target);
        if (success < 1) {
            success = srdice() + srdice();
            GET_OBJ_TIMER(chip) = -1;
        }
        GET_OBJ_VAL(cooker, 9) = (GET_OBJ_VAL(chip, 1) * 24) / success;
        act("The light on $p turns orange as it starts to cook the chip.", TRUE, ch, cooker, 0, TO_ROOM);
        act("The light on $p turns orange as it starts to cook the chip.", TRUE, ch, cooker, 0, TO_CHAR);
    }
}

void part_design(struct char_data *ch, struct obj_data *part) {

    if (!GET_OBJ_VAL(part, 3) || GET_OBJ_TYPE(part) != ITEM_PART)
        send_to_char(ch, "You don't need to design that part.\r\n");
    else if (GET_OBJ_VAL(part, 3) > 0) {
        if (GET_OBJ_VAL(part, 7) != GET_IDNUM(ch))
            send_to_char(ch, "Someone is already working on that part.\r\n");
        else {
            send_to_char(ch, "You continue to design that project.\r\n");
            AFF_FLAGS(ch).SetBit(AFF_PART_DESIGN);
            ch->char_specials.programming = part;
        }
    } else {
        int target = GET_OBJ_VAL(part, 2)/2, skill = get_skill(ch, SKILL_CYBERTERM_DESIGN, target);
        GET_OBJ_VAL(part, 3) = GET_OBJ_VAL(part, 2) * 2;
        GET_OBJ_VAL(part, 5) = success_test(skill, target) << 1;
        GET_OBJ_VAL(part, 7) = GET_IDNUM(ch);
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
        send_to_char("You stop work.\r\n", ch);
      } else
        send_to_char(ch, "What do you want to build?\r\n");
      return;
    }
    if (GET_POS(ch) > POS_SITTING) {
        send_to_char(ch, "You have to be sitting to do that.\r\n");
        return;
    }
    if (IS_WORKING(ch)) {
        send_to_char(TOOBUSY, ch);
        return;
    }
    half_chop(argument, arg1, buf);
    half_chop(buf, arg2, arg3);
    if (!(obj = get_obj_in_list_vis(ch, arg1, ch->carrying))) {
        if (!(obj = get_obj_in_list_vis(ch, arg1, world[ch->in_room].contents))) {
            if (!str_cmp(arg1, "circle")) {
                for (obj = world[ch->in_room].contents; obj; obj = obj->next_content)
                    if (GET_OBJ_TYPE(obj) == ITEM_MAGIC_TOOL && (GET_OBJ_VAL(obj, 0) == TYPE_CIRCLE || GET_OBJ_VAL(obj, 0) == TYPE_LODGE)) {
                        send_to_char("There is already a lodge or a hermetic circle here.\r\n", ch);
                        return;
                    }
                if (!*arg2 || !*arg3)
                    send_to_char("Draw the circle at what force and to what element?\r\n", ch);
                else
                    circle_build(ch, arg3, atoi(arg2));
                return;
            } else if (!str_cmp(arg1, "lodge")) {
                for (obj = world[ch->in_room].contents; obj; obj = obj->next_content)
                    if (GET_OBJ_TYPE(obj) == ITEM_MAGIC_TOOL && (GET_OBJ_VAL(obj, 0) == TYPE_CIRCLE || GET_OBJ_VAL(obj, 0) == TYPE_LODGE)) {
                        send_to_char("There is already a lodge or a hermetic circle here.\r\n", ch);
                        return;
                    }
                if (!*arg2)
                    send_to_char("What force do you wish that lodge to be?\r\n", ch);
                else
                    lodge_build(ch, atoi(arg2));
                return;
            }
            send_to_char(ch, "You don't seem to have that part.\r\n");
        } else if (GET_OBJ_TYPE(obj) == ITEM_MAGIC_TOOL) {
            if (!GET_OBJ_VAL(obj, 9))
                send_to_char("It has already been completed!\r\n", ch);
            else if (GET_OBJ_VAL(obj, 3) != GET_IDNUM(ch))
                send_to_char("That's not yours!\r\n", ch);
            else if (GET_OBJ_VAL(obj, 0) == TYPE_LODGE) {
                AFF_FLAGS(ch).SetBit(AFF_LODGE);
                GET_BUILDING(ch) = obj;
                send_to_char(ch, "You continue working on your lodge.\r\n");
                act("$n continues to build $s lodge.", FALSE, ch, 0, 0, TO_ROOM);
            } else if (GET_OBJ_VAL(obj, 0) == TYPE_CIRCLE) {
                AFF_FLAGS(ch).SetBit(AFF_CIRCLE);
                GET_BUILDING(ch) = obj;
                send_to_char(ch, "You continue working on %s.\r\n", GET_OBJ_NAME(obj));
                act("$n continues to draw the hermetic circle.", FALSE, ch, 0, 0, TO_ROOM);
            } else
                send_to_char("You can't build that!\r\n", ch);
        } else
            send_to_char("You can't build that!\r\n", ch);
        return;

    } else if (GET_OBJ_TYPE(obj) == ITEM_GUN_AMMO) {
      ammo_build(ch, obj);
      return;
    } else if (!(deck = get_obj_in_list_vis(ch, arg2, ch->carrying))) {
        send_to_char(ch, "You don't have that deck.\r\n");
        return;
    }
    if (GET_OBJ_VAL(obj, 0) == PART_MPCP && GET_OBJ_VAL(deck, 0)) {
        struct obj_data *temp = deck->contains;
        if (GET_OBJ_VAL(obj, 2) != GET_OBJ_VAL(deck, 0))
            for (; temp; temp = temp->next_content)
                if (GET_OBJ_TYPE(temp) == ITEM_PART && GET_OBJ_VAL(obj, 2) == GET_OBJ_VAL(temp, 2))
                    break;
        if (!temp) {
            send_to_char(ch, "That part is not designed for the same MPCP as this deck.\r\n");
            return;
        }
    }
    if (GET_OBJ_TYPE(obj) != ITEM_PART || GET_OBJ_TYPE(deck) != ITEM_CUSTOM_DECK)
        send_to_char(ch, "You can't build a part out of that.\r\n");
    else if (GET_OBJ_VAL(obj, 3))
        send_to_char(ch, "You must make a design for that part first.\r\n");
    else if (GET_OBJ_VAL(obj, 0) != PART_MPCP && (GET_OBJ_VAL(obj, 2) != GET_OBJ_VAL(deck, 0) && GET_OBJ_VAL(deck, 0)) &&
             (parts[GET_OBJ_VAL(obj, 0)].design >= 0 || GET_OBJ_VAL(obj, 0) == PART_ACTIVE ||
              GET_OBJ_VAL(obj, 0) == PART_STORAGE || GET_OBJ_VAL(obj, 0) == PART_MATRIX_INTERFACE))
        send_to_char(ch, "That part is not designed for the same MPCP as this deck.\r\n");
    else if (GET_OBJ_VAL(obj, 7) != GET_IDNUM(ch) && GET_OBJ_VAL(obj, 7) > 0)
        send_to_char(ch, "Someone else has already started on that part.\r\n");
    else {
        struct obj_data *workshop = NULL;
        int kit = 0, target = -GET_OBJ_VAL(obj, 5), duration = 0, skill = 0;
        if (has_kit(ch, TYPE_MICROTRONIC))
            kit = TYPE_KIT;
        if ((workshop = find_workshop(ch, TYPE_MICROTRONIC)) && GET_OBJ_VAL(workshop, 0) > kit)
            kit = GET_OBJ_VAL(workshop, 0);
        if (kit < parts[GET_OBJ_VAL(obj, 0)].tools) {
            send_to_char(ch, "You don't have the right tools for that job.\r\n");
            return;
        } else if (parts[GET_OBJ_VAL(obj, 0)].tools == TYPE_KIT) {
            if (kit == TYPE_SHOP)
                target--;
            else if (kit == TYPE_FACILITY)
                target -= 3;
        } else if (parts[GET_OBJ_VAL(obj, 0)].tools == TYPE_SHOP && kit == TYPE_FACILITY)
            target--;
        if (GET_OBJ_VAL(obj, 0) == PART_BOD || GET_OBJ_VAL(obj, 0) == PART_SENSOR || GET_OBJ_VAL(obj, 0) == PART_MASKING || GET_OBJ_VAL(obj, 0) == PART_EVASION) {
            int total = GET_OBJ_VAL(obj, 1);
            for (struct obj_data *part = deck->contains; part; part = part->next_content)
                if (GET_OBJ_TYPE(part) == ITEM_PART && (GET_OBJ_VAL(part, 0) == PART_BOD || GET_OBJ_VAL(part, 0) == PART_SENSOR || GET_OBJ_VAL(part, 0) == PART_MASKING || GET_OBJ_VAL(part, 0) == PART_EVASION))
                    total += GET_OBJ_VAL(part, 1);
            if (total > GET_OBJ_VAL(deck, 0) * 3) {
                send_to_char("Persona programs are limited to three times the MPCP rating of the deck.\r\n", ch);
                return;
            }
        }
        for (struct obj_data *part = deck->contains; part; part = part->next_content)
            if (GET_OBJ_TYPE(part) == TYPE_PARTS && (GET_OBJ_VAL(part, 0) == GET_OBJ_VAL(obj, 0) || (GET_OBJ_VAL(part, 0) ==
                    PART_ASIST_HOT && GET_OBJ_VAL(obj, 0) == PART_ASIST_COLD) || (GET_OBJ_VAL(part, 0) == PART_ASIST_COLD && GET_OBJ_VAL(obj, 0) == PART_ASIST_HOT))) {
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
                        if (GET_OBJ_VAL(soft, 0) == parts[GET_OBJ_VAL(obj, 0)].software && GET_OBJ_TIMER(soft) == 1 && GET_OBJ_VAL(soft, 1) == GET_OBJ_VAL(obj, 1))
                            break;
                    if (!soft) {
                        send_to_char(ch, "You need to program and cook the required software for that first.\r\n");
                        return;
                    }
                }
                struct obj_data *chips = NULL, *part = NULL;
                for (struct obj_data *find = world[ch->in_room].contents; find; find = find->next_content)
                    if (GET_OBJ_TYPE(find) == ITEM_DECK_ACCESSORY && GET_OBJ_VAL(find, 0) == TYPE_PARTS) {
                        if (GET_OBJ_VAL(find, 1) && GET_OBJ_COST(find) >= GET_OBJ_VAL(obj, 9))
                            chips = find;
                        else if (!GET_OBJ_VAL(find, 1) && GET_OBJ_COST(find) >= GET_OBJ_VAL(obj, 8))
                            part = find;
                    }
                for (struct obj_data *find = ch->carrying; find; find = find->next_content)
                    if (GET_OBJ_TYPE(find) == ITEM_DECK_ACCESSORY && GET_OBJ_VAL(find, 0) == TYPE_PARTS) {
                        if (GET_OBJ_VAL(find, 1) && GET_OBJ_COST(find) >= GET_OBJ_VAL(obj, 9))
                            chips = find;
                        else if (!GET_OBJ_VAL(find, 1) && GET_OBJ_COST(find) >= GET_OBJ_VAL(obj, 8))
                            part = find;
                    }
                if (GET_OBJ_VAL(obj, 8)) {
                    if (!part) {
                        send_to_char(ch, "You don't have enough materials for that part.\r\n");
                        return;
                    } else {
                        GET_OBJ_COST(part) -= GET_OBJ_VAL(obj, 8);
                        if (!GET_OBJ_COST(part))
                            extract_obj(part);
                    }
                } else if (GET_OBJ_VAL(obj, 9)) {
                    if (!chips) {
                        send_to_char(ch, "You don't have enough optical chips for that part.\r\n");
                        return;
                    } else {
                        GET_OBJ_COST(chips) -= GET_OBJ_VAL(obj, 9);
                        if (!GET_OBJ_COST(chips))
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
  if (AFF_FLAGS(ch).IsSet(AFF_CIRCLE)) {
    send_to_char(ch, "The hermetic circle you are working on is about %d%% completed.\r\n", 
                       (int)(((float)((GET_OBJ_VAL(ch->char_specials.programming, 1) * 60) -
                       GET_OBJ_VAL(ch->char_specials.programming, 9)) / (float)(GET_OBJ_VAL(ch->char_specials.programming, 1) * 60)) * 100));
  } else if (AFF_FLAGS(ch).IsSet(AFF_LODGE)) {
    send_to_char(ch, "The lodge you are working on is about %d%% completed.\r\n", (int)(((float)((GET_OBJ_VAL(ch->char_specials.programming, 1) * 300) -
                     GET_OBJ_VAL(ch->char_specials.programming, 9)) / (float)(GET_OBJ_VAL(ch->char_specials.programming, 1) * 300)) * 100));
  } else if (AFF_FLAGS(ch).IsSet(AFF_PACKING)) {
    for (struct obj_data *obj = world[ch->in_room].contents; obj; obj = obj->next_content)
      if (GET_OBJ_TYPE(obj) == ITEM_WORKSHOP && GET_OBJ_VAL(obj, 3)) {
        send_to_char(ch, "You are about %d%% of the way through%spacking %s.\r\n",
                          (int)((float)((3.0-GET_OBJ_VAL(obj, 3)) / 3)*100), GET_OBJ_VAL(obj, 2) ? " " : " un", 
                         GET_OBJ_NAME(obj));
        break;
      } 
  } else if (AFF_FLAGS(ch).IsSet(AFF_BONDING)) {
    send_to_char(ch, "You are about %d%% of the way through bonding %s.\r\n", 
                 (int)(((float)(GET_OBJ_VAL(GET_BUILDING(ch), 1) - GET_OBJ_VAL(GET_BUILDING(ch), 9)) / GET_OBJ_VAL(GET_BUILDING(ch), 1) * 60)*100),
                 GET_OBJ_NAME(GET_BUILDING(ch)));
  } else if (AFF_FLAGS(ch).IsSet(AFF_PROGRAM)) {
    send_to_char(ch, "You are about %d%% of the way through programming %s.\r\n", 
           (int)(((float)(GET_OBJ_TIMER(GET_BUILDING(ch)) - GET_OBJ_VAL(GET_BUILDING(ch), 5)) / 
           GET_OBJ_TIMER(GET_BUILDING(ch))) * 100), GET_OBJ_NAME(GET_BUILDING(ch)));
  } else if (AFF_FLAGS(ch).IsSet(AFF_PART_BUILD)) {
    send_to_char(ch, "You are about %d%% of the way through building %s.\r\n",
           (int)(((float)(GET_OBJ_VAL(GET_BUILDING(ch), 10) - GET_OBJ_VAL(GET_BUILDING(ch), 4)) / 
           GET_OBJ_VAL(GET_BUILDING(ch), 10)) * 100), GET_OBJ_NAME(GET_BUILDING(ch)));
  } else if (AFF_FLAGS(ch).IsSet(AFF_PART_DESIGN)) {
  } else if (AFF_FLAGS(ch).IsSet(AFF_DESIGN)) {
    send_to_char(ch, "You are about %d%% of the way through designing %s.\r\n", 
           (int)(((float)(GET_OBJ_TIMER(GET_BUILDING(ch)) - GET_OBJ_VAL(GET_BUILDING(ch), 4)) / GET_OBJ_TIMER(GET_BUILDING(ch))) * 100),
           GET_OBJ_NAME(GET_BUILDING(ch)));
  } else if (AFF_FLAGS(ch).IsSet(AFF_CONJURE)) {
  } else if (AFF_FLAGS(ch).IsSet(AFF_SPELLDESIGN)) {
    int timeleft = GET_OBJ_VAL(ch->char_specials.programming, 6);
    if (GET_OBJ_TIMER(ch->char_specials.programming) == -3)
      timeleft *= 2;
    send_to_char(ch, "You are about %d%% done designing %s.\r\n", (int)(((float)timeleft / (float)GET_OBJ_VAL(ch->char_specials.programming, 7)) *-100 + 100), GET_OBJ_NAME(ch->char_specials.programming));
  } else if (AFF_FLAGS(ch).IsSet(AFF_AMMOBUILD)) {
    send_to_char(ch, "You are about %d%% of the way through making %s.\r\n",
           (int)(((float)(GET_OBJ_VAL(GET_BUILDING(ch), 10) - GET_OBJ_VAL(GET_BUILDING(ch), 4)) / 
           GET_OBJ_VAL(GET_BUILDING(ch), 10)) * 100), GET_OBJ_NAME(GET_BUILDING(ch)));
  } else
    send_to_char("You are not working on anything at this time.\r\n", ch);
}
