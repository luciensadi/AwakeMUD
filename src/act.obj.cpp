/*
************************************************************************
*   File: act.obj.c                                    Part of CircleMUD  *
*  Usage: object handling routines                                        *
*                                                                         *
*  All rights reserved.  See license.doc for complete information.        *
*                                                                         *
*  Copyright (C) 1993, 94 by the Trustees of the Johns Hopkins University *
*  CircleMUD is based on DikuMUD, Copyright (C) 1990, 1991.               *
************************************************************************ */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "structs.hpp"
#include "utils.hpp"
#include "comm.hpp"
#include "interpreter.hpp"
#include "handler.hpp"
#include "db.hpp"
#include "awake.hpp"
#include "constants.hpp"
#include "newmatrix.hpp"
#include "newdb.hpp"
#include "limits.hpp"
#include "ignore_system.hpp"

/* extern variables */
extern int drink_aff[][3];
extern PCIndex playerDB;

// extern funcs
extern char *get_token(char *, char*);
extern int modify_target(struct char_data *ch);
extern int return_general(int skill_num);
extern bool check_quest_delivery(struct char_data *ch, struct char_data *mob, struct obj_data *obj);
extern void check_quest_delivery(struct char_data *ch, struct obj_data *obj);
extern void check_quest_destroy(struct char_data *ch, struct obj_data *obj);
extern void dominator_mode_switch(struct char_data *ch, struct obj_data *obj, int mode);
extern float get_bulletpants_weight(struct char_data *ch);
extern int calculate_vehicle_weight(struct veh_data *veh);

// Corpse saving externs.
extern void write_world_to_disk(int vnum);

extern SPECIAL(fence);
extern SPECIAL(hacker);
extern SPECIAL(fixer);
extern SPECIAL(mageskill_herbie);

void calc_weight(struct char_data *ch);

SPECIAL(weapon_dominator);

extern WSPEC(monowhip);

int wear_bitvectors[] = {
                          ITEM_WEAR_TAKE, ITEM_WEAR_HEAD, ITEM_WEAR_EYES, ITEM_WEAR_EAR,
                          ITEM_WEAR_EAR, ITEM_WEAR_FACE, ITEM_WEAR_NECK, ITEM_WEAR_NECK,
                          ITEM_WEAR_BACK, ITEM_WEAR_ABOUT, ITEM_WEAR_BODY, ITEM_WEAR_UNDER,
                          ITEM_WEAR_ARMS, ITEM_WEAR_ARM, ITEM_WEAR_ARM, ITEM_WEAR_WRIST,
                          ITEM_WEAR_WRIST, ITEM_WEAR_HANDS, ITEM_WEAR_WIELD, ITEM_WEAR_HOLD, ITEM_WEAR_SHIELD,
                          ITEM_WEAR_FINGER, ITEM_WEAR_FINGER, ITEM_WEAR_FINGER, ITEM_WEAR_FINGER,
                          ITEM_WEAR_FINGER, ITEM_WEAR_FINGER, ITEM_WEAR_FINGER, ITEM_WEAR_FINGER,
                          ITEM_WEAR_BELLY, ITEM_WEAR_WAIST, ITEM_WEAR_THIGH, ITEM_WEAR_THIGH,
                          ITEM_WEAR_LEGS, ITEM_WEAR_ANKLE, ITEM_WEAR_ANKLE, ITEM_WEAR_SOCK, ITEM_WEAR_FEET };

bool search_cyberdeck(struct obj_data *cyberdeck, struct obj_data *program)
{
  struct obj_data *temp;

  for (temp = cyberdeck->contains; temp; temp = temp->next_content)
    if ((GET_OBJ_TYPE(temp) == ITEM_PROGRAM && GET_OBJ_TYPE(program) == ITEM_PROGRAM && GET_OBJ_VAL(temp, 0) == GET_OBJ_VAL(program, 0)) ||
        (GET_OBJ_TYPE(temp) == ITEM_DECK_ACCESSORY && GET_OBJ_TYPE(program) == ITEM_DECK_ACCESSORY && GET_OBJ_VAL(temp, 1) == GET_OBJ_VAL(program, 1)))
      return TRUE;

  return FALSE;
}

void perform_put(struct char_data *ch, struct obj_data *obj, struct obj_data *cont)
{
  if (obj == ch->char_specials.programming)
  {
    send_to_char(ch, "You can't put something you are working on inside something.\r\n");
    return;
  }
  if (GET_OBJ_TYPE(cont) == ITEM_WORN)
  {
    if (GET_OBJ_TYPE(obj) == ITEM_SPELL_FORMULA || GET_OBJ_TYPE(obj) == ITEM_DESIGN || GET_OBJ_TYPE(obj) == ITEM_PART)  {
      act("You can't store spell formulas, designs, or parts in $P.", FALSE, ch, obj, cont, TO_CHAR);
      return;
    }

    if (GET_OBJ_TYPE(obj) == ITEM_HOLSTER || GET_OBJ_TYPE(obj) == ITEM_QUIVER) {
      if (GET_OBJ_VAL(cont, 0)) {
        GET_OBJ_VAL(cont, 0)--;
        GET_OBJ_TIMER(obj) = 0;
      } else {
        act("$p won't fit in $P.", FALSE, ch, obj, cont, TO_CHAR);
        return;
      }
    } else if (GET_OBJ_TYPE(obj) == ITEM_GUN_MAGAZINE) {
      if (GET_OBJ_VAL(cont, 1)) {
        GET_OBJ_VAL(cont, 1)--;
        GET_OBJ_TIMER(obj) = 1;
      } else if (GET_OBJ_VAL(cont, 4)) {
        GET_OBJ_VAL(cont, 4)--;
        GET_OBJ_TIMER(obj) = 4;
      } else {
        act("$p won't fit in $P.", FALSE, ch, obj, cont, TO_CHAR);
        return;
      }
    } else if (GET_OBJ_TYPE(obj) == ITEM_WEAPON) {
      if (GET_OBJ_VAL(obj, 3) == TYPE_HAND_GRENADE) {
        if (GET_OBJ_VAL(cont, 2)) {
          GET_OBJ_VAL(cont, 2)--;
          GET_OBJ_TIMER(obj) = 2;
        } else if (GET_OBJ_VAL(cont, 4)) {
          GET_OBJ_VAL(cont, 4)--;
          GET_OBJ_TIMER(obj) = 4;
        } else {
          act("$p won't fit in $P.", FALSE, ch, obj, cont, TO_CHAR);
          return;
        }
      } else if (GET_OBJ_VAL(obj, 3) == TYPE_SHURIKEN) {
        if (GET_OBJ_VAL(cont, 3)) {
          GET_OBJ_VAL(cont, 3)--;
          GET_OBJ_TIMER(obj) = 3;
        } else if (GET_OBJ_VAL(cont, 4)) {
          GET_OBJ_VAL(cont, 4)--;
          GET_OBJ_TIMER(obj) = 4;
        } else {
          act("$p won't fit in $P.", FALSE, ch, obj, cont, TO_CHAR);
          return;
        }
      } else {
        act("$p won't fit in $P.", FALSE, ch, obj, cont, TO_CHAR);
        return;
      }
    } else if (GET_OBJ_VAL(cont, 4)) {
      if (GET_OBJ_WEIGHT(obj) <= 1) {
        GET_OBJ_VAL(cont, 4)--;
        GET_OBJ_TIMER(obj) = 4;
      } else {
        act("$p won't fit in $P.", FALSE, ch, obj, cont, TO_CHAR);
        return;
      }
    } else {
      act("$p won't fit in $P.", FALSE, ch, obj, cont, TO_CHAR);
      return;
    }

    if (obj->in_obj)
      obj_from_obj(obj);
    else
      obj_from_char(obj);
    obj_to_obj(obj, cont);

    act("You put $p in $P.", FALSE, ch, obj, cont, TO_CHAR);
    act("$n puts $p in $P.", FALSE, ch, obj, cont, TO_ROOM);
    return;
  }

  if (GET_OBJ_TYPE(cont) == ITEM_QUIVER) {
    if ((GET_OBJ_VAL(cont, 1) == 0 && !(GET_OBJ_TYPE(obj) == ITEM_MISSILE && GET_OBJ_VAL(obj, 0) == 0)) ||
        (GET_OBJ_VAL(cont, 1) == 1 && !(GET_OBJ_TYPE(obj) == ITEM_MISSILE && GET_OBJ_VAL(obj, 0) == 1)) ||
        (GET_OBJ_VAL(cont, 1) == 2 && !(GET_OBJ_TYPE(obj) == ITEM_WEAPON && GET_OBJ_VAL(obj, 3) == TYPE_SHURIKEN)) ||
        (GET_OBJ_VAL(cont, 1) == 3 && !(GET_OBJ_TYPE(obj) == ITEM_WEAPON && GET_OBJ_VAL(obj, 3) == TYPE_THROWING_KNIFE)))
      return;
    if (GET_OBJ_VAL(cont, 2) >= GET_OBJ_VAL(cont, 0))
      act("$p won't fit in $P.", FALSE, ch, obj, cont, TO_CHAR);
    else {
      obj_from_char(obj);
      obj_to_obj(obj, cont);

      GET_OBJ_VAL(cont, 2)++;
      act("You put $p in $P.", FALSE, ch, obj, cont, TO_CHAR);
      act("$n puts $p in $P.", TRUE, ch, obj, cont, TO_ROOM);
      if ( (!IS_NPC(ch) && access_level( ch, LVL_BUILDER ))
           || IS_OBJ_STAT( obj, ITEM_EXTRA_WIZLOAD) ) {
        char *representation = generate_new_loggable_representation(obj);
        snprintf(buf, sizeof(buf), "%s puts in (%ld) %s [restring: %s]: %s", GET_CHAR_NAME(ch),
                GET_OBJ_VNUM( cont ), cont->text.name, cont->restring ? cont->restring : "none",
                representation);
        mudlog(buf, ch, IS_OBJ_STAT(obj, ITEM_EXTRA_WIZLOAD) ? LOG_WIZITEMLOG : LOG_CHEATLOG, TRUE);
        delete [] representation;
      }
    }
    return;
  }

  if (GET_OBJ_TYPE(cont) == ITEM_KEYRING) {
    if (GET_OBJ_TYPE(obj) != ITEM_KEY) {
      act("You can only put keys on $P.", FALSE, ch, obj, cont, TO_CHAR);
      return;
    }

    // Rule out some of the weird keys.
    switch (GET_OBJ_VNUM(obj)) {
      case OBJ_ENCYCLOPEDIA_LABELED_O:
      case OBJ_OPTICAL_CHIP_KEY:
      case OBJ_FIBEROPTIC_CRYSTAL:
      case OBJ_UNFINISHED_EQUATION:
      case OBJ_SCANEYE:
        send_to_char(ch, "You stare blankly at %s, unable to figure out how to thread it onto a keyring without putting holes in it.\r\n", decapitalize_a_an(GET_OBJ_NAME(obj)));
        return;
      case OBJ_EYEBALL_KEY:
      case OBJ_ELECTRONIC_EYEBALL_KEY:
        send_to_char(ch, "%s squishes unpleasantly between your fingers as you try to wrap the remnants of the optic nerve around the keyring.\r\n", capitalize(GET_OBJ_NAME(obj)));
        return;
    }

    // Previously, we weight-limited the keyring, but that's no fun.
    /*
    if (GET_OBJ_WEIGHT(cont) + GET_OBJ_WEIGHT(obj) > MAX_KEYRING_WEIGHT) {
      act("$P's ring isn't strong enough to hold everything you're trying to put on it.", FALSE, ch, obj, cont, TO_CHAR);
      return;
    }
    */
  } else if (GET_OBJ_WEIGHT(cont) + GET_OBJ_WEIGHT(obj) > (GET_OBJ_VAL(cont, 0) + get_proto_weight(cont))) {
    act("$p won't fit in $P.", FALSE, ch, obj, cont, TO_CHAR);
    return;
  }

  if (obj->in_obj)
    obj_from_obj(obj);
  else
    obj_from_char(obj);
  obj_to_obj(obj, cont);

  act("You put $p in $P.", FALSE, ch, obj, cont, TO_CHAR);
  act("$n puts $p in $P.", TRUE, ch, obj, cont, TO_ROOM);
  if ( (!IS_NPC(ch) && access_level( ch, LVL_BUILDER ))
       || IS_OBJ_STAT( obj, ITEM_EXTRA_WIZLOAD) ) {
    char *representation = generate_new_loggable_representation(obj);
    snprintf(buf, sizeof(buf), "%s puts in (%ld) %s [restring: %s]: %s", GET_CHAR_NAME(ch),
            GET_OBJ_VNUM( cont ), cont->text.name, cont->restring ? cont->restring : "none",
            representation);
    mudlog(buf, ch, IS_OBJ_STAT(obj, ITEM_EXTRA_WIZLOAD) ? LOG_WIZITEMLOG : LOG_CHEATLOG, TRUE);
    delete [] representation;
  }
}

void perform_put_cyberdeck(struct char_data * ch, struct obj_data * obj,
                           struct obj_data * cont)
{
  if (GET_OBJ_TYPE(cont) == ITEM_DECK_ACCESSORY)
  {
    if (GET_DECK_ACCESSORY_TYPE(cont) != TYPE_COMPUTER) {
      send_to_char(ch, "You can't install anything into %s.\r\n", GET_OBJ_NAME(cont));
      return;
    }

    if (cont->carried_by) {
      send_to_char(ch, "%s won't work while carried, you'll have to drop it.\r\n", capitalize(GET_OBJ_NAME(cont)));
      return;
    }

    if (GET_OBJ_TYPE(obj) != ITEM_DESIGN && GET_OBJ_TYPE(obj) != ITEM_PROGRAM && !(GET_OBJ_TYPE(obj) == ITEM_PROGRAM && GET_OBJ_TIMER(obj))) {
      send_to_char(ch, "You can't install %s onto a personal computer; you can only install uncooked programs and designs.\r\n", GET_OBJ_NAME(obj));
      return;
    }

    int space_required = 0;
    if (GET_OBJ_TYPE(obj) == ITEM_PROGRAM) {
      space_required = GET_PROGRAM_SIZE(obj);
    } else {
      space_required = (int) GET_DESIGN_SIZE(obj) * 1.1;
    }
    int free_space = GET_DECK_ACCESSORY_COMPUTER_MAX_MEMORY(cont) - GET_DECK_ACCESSORY_COMPUTER_USED_MEMORY(cont);

    if (free_space < space_required) {
      send_to_char(ch, "%s doesn't seem to fit on %s-- it takes %d megapulses, but there are only %d available.\r\n",
                   capitalize(GET_OBJ_NAME(obj)),
                   decapitalize_a_an(GET_OBJ_NAME(cont)),
                   space_required,
                   free_space
                  );
      return;
    }

    GET_DECK_ACCESSORY_COMPUTER_USED_MEMORY(cont) += space_required;

    obj_from_char(obj);
    obj_to_obj(obj, cont);
    send_to_char(ch, "You install %s onto %s.\r\n", GET_OBJ_NAME(obj), GET_OBJ_NAME(cont));
    act("$n installs $p on $P.", TRUE, ch, obj, cont, TO_ROOM);
    return;
  } else if (GET_OBJ_TYPE(obj) == ITEM_DECK_ACCESSORY)
  {
    switch (GET_DECK_ACCESSORY_TYPE(obj)) {
      case TYPE_FILE:
        if (GET_OBJ_VAL(cont, 5) + GET_DECK_ACCESSORY_FILE_SIZE(obj) > GET_OBJ_VAL(cont, 3)) {
          act("$p takes up too much memory to be uploaded into $P.", FALSE, ch, obj, cont, TO_CHAR);
          return;
        }
        obj_from_char(obj);
        obj_to_obj(obj, cont);
        GET_OBJ_VAL(cont, 5) += GET_DECK_ACCESSORY_FILE_SIZE(obj);
        act("You upload $p in $P.", FALSE, ch, obj, cont, TO_CHAR);
        return;
      case TYPE_UPGRADE:
        if (GET_OBJ_VAL(obj, 1) != 3) {
          send_to_char(ch, "You can't seem to fit %s into %s.\r\n", GET_OBJ_NAME(obj), GET_OBJ_NAME(cont));
          return;
        }
        obj_from_char(obj);
        obj_to_obj(obj, cont);
        act("You fit $p into a FUP in $P.", FALSE, ch, obj, cont, TO_CHAR);
        return;
      default:
        send_to_char(ch, "You can't seem to fit %s into %s.\r\n", GET_OBJ_NAME(obj), GET_OBJ_NAME(cont));
        return;
    }
  }
  // Prevent installing persona firmware into a store-bought deck.
  if (GET_OBJ_VNUM(obj) == OBJ_BLANK_PROGRAM
      && (GET_OBJ_VAL(obj, 0) == SOFT_BOD
          || GET_OBJ_VAL(obj, 0) == SOFT_SENSOR
          || GET_OBJ_VAL(obj, 0) == SOFT_MASKING
          || GET_OBJ_VAL(obj, 0) == SOFT_EVASION)) {
    if (GET_OBJ_VNUM(cont) == OBJ_CUSTOM_CYBERDECK_SHELL) {
      send_to_char(ch, "%s is firmware, you'll have to BUILD it into the deck along with the matching chip.\r\n", GET_OBJ_NAME(obj));
    } else {
      send_to_char(ch, "%s is firmware for a custom cyberdeck persona chip. It's not compatible with store-bought decks.\r\n",
                   GET_OBJ_NAME(obj));
    }
    return;
  }
  else if (!GET_OBJ_TIMER(obj) && GET_OBJ_VNUM(obj) == OBJ_BLANK_PROGRAM)
    send_to_char(ch, "You'll have to cook %s before you can install it.\r\n", GET_OBJ_NAME(obj));
  else if (GET_CYBERDECK_MPCP(cont) == 0 || GET_CYBERDECK_IS_INCOMPLETE(cont))
    display_cyberdeck_issues(ch, cont);
  else if (GET_OBJ_VAL(cont, 5) + GET_OBJ_VAL(obj, 2) > GET_OBJ_VAL(cont, 3))
    act("$p takes up too much memory to be installed into $P.", FALSE,
        ch, obj, cont, TO_CHAR);
  else if (search_cyberdeck(cont, obj))
    act("You already have a similar program installed in $P.", FALSE, ch, obj, cont, TO_CHAR);
  else
  {
    obj_from_char(obj);
    obj_to_obj(obj, cont);
    act("You install $p in $P.", FALSE, ch, obj, cont, TO_CHAR);
    GET_OBJ_VAL(cont, 5) += GET_OBJ_VAL(obj, 2);
  }
}

/* The following put modes are supported by the code below:

        1) put <object> <container>
        2) put all.<object> <container>
        3) put all <container>

        <container> must be in inventory or on ground.
        all objects to be put into container must be in inventory.
*/

ACMD(do_put)
{
  char arg1[MAX_INPUT_LENGTH];
  char arg2[MAX_INPUT_LENGTH];
  struct obj_data *obj, *next_obj, *cont;
  struct char_data *tmp_char;
  int obj_dotmode, cont_dotmode, found = 0;
  bool cyberdeck = FALSE;
  if (IS_WORKING(ch)) {
    send_to_char(TOOBUSY, ch);
    return;
  }
  if (IS_ASTRAL(ch)) {
    send_to_char("Astral beings can't touch things!\r\n", ch);
    return;
  }

  two_arguments(argument, arg1, arg2);
  obj_dotmode = find_all_dots(arg1, sizeof(arg1));
  cont_dotmode = find_all_dots(arg2, sizeof(arg2));
  cont_dotmode++;

  if (subcmd == SCMD_INSTALL)
    cyberdeck = TRUE;

  if (!*arg1) {
    send_to_char(ch, "%s what in what?\r\n", (cyberdeck ? "Install" : "Put"));
    return;
  }

  if (obj_dotmode != FIND_INDIV) {
    snprintf(buf, sizeof(buf), "You can only %s %s into one %s at a time.\r\n",
            (cyberdeck ? "install" : "put"), (cyberdeck ? "programs" : "things"),
            (cyberdeck ? "cyberdeck" : "container"));
    send_to_char(buf, ch);
    return;
  }

  if (!*arg2) {
    snprintf(buf, sizeof(buf), "What do you want to %s %s in?\r\n", (cyberdeck ? "install" : "put"),
            ((obj_dotmode == FIND_INDIV) ? "it" : "them"));
    send_to_char(buf, ch);
    return;
  }

  if (!str_cmp(arg2, "finger")) {
    for (cont = ch->cyberware; cont; cont = cont->next_content)
      if (GET_CYBERWARE_TYPE(cont) == CYB_FINGERTIP)
        break;
    if (!cont) {
      send_to_char("You don't have a fingertip compartment.\r\n", ch);
      return;
    }
    if (cont->contains) {
      send_to_char("Your fingertip compartment is already full.\r\n", ch);
      return;
    }
    if (!(obj = get_obj_in_list_vis(ch, arg1, ch->carrying))) {
      send_to_char(ch, "You aren't carrying %s %s.\r\n", AN(arg1), arg1);
      return;
    }
    if (GET_OBJ_TYPE(obj) != ITEM_PROGRAM
        && (GET_OBJ_TYPE(obj) != ITEM_DRUG || (GET_OBJ_VAL(obj, 0) != DRUG_CRAM && GET_OBJ_VAL(obj, 0) != DRUG_PSYCHE))
        && !(IS_MONOWHIP(obj))) {
      send_to_char(ch, "%s doesn't fit in your fingertip compartment.\r\n", GET_OBJ_NAME(obj));
      return;
    }
    obj_from_char(obj);
    obj_to_obj(obj, cont);
    send_to_char(ch, "You slip %s into your fingertip compartment.\r\n", GET_OBJ_NAME(obj));
    act("$n slips $p into $s fingertip compartment.\r\n", TRUE, ch, obj, 0, TO_ROOM);
    return;
  }

  if (!str_cmp(arg2, "body")) {
    for (cont = ch->cyberware; cont; cont = cont->next_content)
      if (GET_OBJ_VAL(cont, 0) == CYB_BODYCOMPART)
        break;
    if (!cont) {
      send_to_char("You don't have a body compartment.\r\n", ch);
      return;
    }
    if (cont->contains) {
      send_to_char("Your body compartment is already full.\r\n", ch);
      return;
    }
    if (!(obj = get_obj_in_list_vis(ch, arg1, ch->carrying))) {
      send_to_char(ch, "You aren't carrying %s %s.\r\n", AN(arg1), arg1);
      return;
    }
    if (GET_OBJ_WEIGHT(obj) > 1) {
      send_to_char(ch, "%s doesn't fit in your body compartment.\r\n", GET_OBJ_NAME(obj));
      return;
    }
    obj_from_char(obj);
    obj_to_obj(obj, cont);
    send_to_char(ch, "You slip %s into your body compartment.\r\n", GET_OBJ_NAME(obj));
    act("$n slips $p into $s body compartment.\r\n", TRUE, ch, 0, obj, TO_ROOM);
    return;
  }

  if (!str_cmp(arg2, "tooth")) {
    for (cont = ch->cyberware; cont; cont = cont->next_content)
      if (GET_OBJ_VAL(cont, 0) == CYB_TOOTHCOMPARTMENT)
        break;
    if (!cont) {
      send_to_char("You don't have a tooth compartment.\r\n", ch);
      return;
    }
    if (cont->contains) {
      send_to_char("Your tooth compartment is already full.\r\n", ch);
      return;
    }
    if (!(obj = get_obj_in_list_vis(ch, arg1, ch->carrying))) {
      send_to_char(ch, "You aren't carrying %s %s.\r\n", AN(arg1), arg1);
      return;
    }
    if ((GET_OBJ_VAL(cont, 3) && GET_OBJ_TYPE(obj) != ITEM_DRUG) || (GET_OBJ_VAL(obj, 0) != DRUG_CRAM && GET_OBJ_VAL(obj, 0) != DRUG_PSYCHE)) {
      send_to_char("You can only put the drugs cram or psyche in a breakable tooth compartment.\r\n", ch);
      return;
    } else if (!GET_OBJ_VAL(cont, 3) && GET_OBJ_TYPE(obj) != ITEM_PROGRAM && (GET_OBJ_TYPE(obj) == ITEM_DRUG &&
               GET_OBJ_VAL(obj, 0) != DRUG_CRAM && GET_OBJ_VAL(obj, 0) != DRUG_PSYCHE)) {
      send_to_char("You can only fit optical chips in a tooth compartment.\r\n", ch);
      return;
    }
    obj_from_char(obj);
    obj_to_obj(obj, cont);
    send_to_char(ch, "You slip %s into your tooth compartment.\r\n", GET_OBJ_NAME(obj));
    act("$n slips $p into a tooth compartment.\r\n", TRUE, ch, 0, obj, TO_ROOM);
    return;
  }

  generic_find(arg2, FIND_OBJ_EQUIP | FIND_OBJ_INV | FIND_OBJ_ROOM, ch, &tmp_char, &cont);
  if (!cont) {
    struct veh_data *veh = get_veh_list(arg2, ch->in_veh ? ch->in_veh->carriedvehs : ch->in_room->vehicles, ch);
    if (veh) {
      send_to_char(ch, "%s is a vehicle-- you'll have to use the UPGRADE command.\r\n", capitalize(GET_VEH_NAME_NOFORMAT(veh)));
    } else {
      send_to_char(ch, "You don't see %s %s here.\r\n", AN(arg2), arg2);
    }
    return;
  }

  // Combine ammo boxes.
  if (GET_OBJ_TYPE(cont) == ITEM_GUN_AMMO) {
    if (!(obj = get_obj_in_list_vis(ch, arg1, ch->carrying))) {
      send_to_char(ch, "You aren't carrying %s %s.\r\n", AN(arg1), arg1);
      return;
    }

    if (obj == cont) {
      send_to_char(ch, "You cannot combine %s with itself.\r\n", GET_OBJ_NAME(obj));
      return;
    }

    // Restriction: You can't wombo-combo non-ammo into ammo.
    if (GET_OBJ_TYPE(obj) != ITEM_GUN_AMMO) {
      send_to_char(ch, "%s will only accept the contents of other ammo boxes, and %s doesn't qualify.\r\n",
        GET_OBJ_NAME(cont),
        GET_OBJ_NAME(obj)
      );
      return;
    }

    // If it's got a creator set, it's not done yet.
    if (GET_AMMOBOX_INTENDED_QUANTITY(cont) > 0) {
      send_to_char(ch, "%s still has disassembled rounds in it. It needs to be completed first.\r\n", GET_OBJ_NAME(cont));
      return;
    }

    if (GET_AMMOBOX_INTENDED_QUANTITY(obj) > 0) {
      send_to_char(ch, "%s still has disassembled rounds in it. It needs to be completed first.\r\n", GET_OBJ_NAME(obj));
      return;
    }

    // If the weapons don't match, no good.
    if (GET_AMMOBOX_WEAPON(cont) != GET_AMMOBOX_WEAPON(obj)) {
      send_to_char(ch, "You can't combine %s ammo with %s ammo.\r\n",
        weapon_type[GET_AMMOBOX_WEAPON(cont)],
        weapon_type[GET_AMMOBOX_WEAPON(obj)]
      );
      return;
    }

    // If the ammo types don't match, no good.
    if (GET_AMMOBOX_TYPE(cont) != GET_AMMOBOX_TYPE(obj)) {
      send_to_char(ch, "You can't combine %s ammo with %s ammo.\r\n",
        ammo_type[GET_AMMOBOX_TYPE(cont)].name,
        ammo_type[GET_AMMOBOX_TYPE(obj)].name
      );
      return;
    }

    // Combine them. This handles junking of empties, restringing, etc.
    if (!combine_ammo_boxes(ch, obj, cont, TRUE)) {
      send_to_char("Something went wrong. Please reach out to the staff.\r\n", ch);
    }
    return;
  }

  // Combine cyberdeck parts/chips, or combine summoning materials.
  if ((GET_OBJ_TYPE(cont) == ITEM_DECK_ACCESSORY && GET_OBJ_VAL(cont, 0) == TYPE_PARTS) ||
             (GET_OBJ_TYPE(cont) == ITEM_MAGIC_TOOL && GET_OBJ_VAL(cont, 0) == TYPE_SUMMONING)) {
    if (!(obj = get_obj_in_list_vis(ch, arg1, ch->carrying)))
      send_to_char(ch, "You aren't carrying %s %s.\r\n", AN(arg1), arg1);
    else if (obj == cont)
      send_to_char(ch, "You cannot combine %s with itself.\r\n", GET_OBJ_NAME(obj));
    else if (GET_OBJ_TYPE(cont) != GET_OBJ_TYPE(obj) || GET_OBJ_VAL(obj, 0) != GET_OBJ_VAL(cont, 0))
      send_to_char(ch, "You cannot combine %s with %s.\r\n", GET_OBJ_NAME(obj), GET_OBJ_NAME(cont));
    else {
      GET_OBJ_COST(cont) += GET_OBJ_COST(obj);
      send_to_char(ch, "You combine %s and %s.\r\n", GET_OBJ_NAME(cont), GET_OBJ_NAME(obj));
      extract_obj(obj);
    }
    return;
  }

  if (cyberdeck) {
    if (!(GET_OBJ_TYPE(cont) == ITEM_CYBERDECK || GET_OBJ_TYPE(cont) == ITEM_CUSTOM_DECK || GET_OBJ_TYPE(cont) == ITEM_DECK_ACCESSORY)) {
      snprintf(buf, sizeof(buf), "$p is not a cyberdeck.");
      act(buf, FALSE, ch, cont, 0, TO_CHAR);
      return;
    }
  } else if (GET_OBJ_TYPE(cont) != ITEM_CONTAINER && GET_OBJ_TYPE(cont) != ITEM_QUIVER && GET_OBJ_TYPE(cont) != ITEM_WORN && GET_OBJ_TYPE(cont) != ITEM_KEYRING) {
    snprintf(buf, sizeof(buf), "$p is not a container.");
    act(buf, FALSE, ch, cont, 0, TO_CHAR);
    return;
  }

  if (IS_SET(GET_OBJ_VAL(cont, 1), CONT_CLOSED) && (GET_OBJ_TYPE(cont) != ITEM_CYBERDECK && GET_OBJ_TYPE(cont) != ITEM_QUIVER && GET_OBJ_TYPE(cont) != ITEM_WORN && GET_OBJ_TYPE(cont) != ITEM_DECK_ACCESSORY) && GET_OBJ_TYPE(cont) != ITEM_CUSTOM_DECK) {
    send_to_char("You'd better open it first!\r\n", ch);
    return;
  }

  if (obj_dotmode == FIND_INDIV) {  /* put <obj> <container> */
    if (!(obj = get_obj_in_list_vis(ch, arg1, ch->carrying))) {
      send_to_char(ch, "You aren't carrying %s %s.\r\n", AN(arg1), arg1);
      return;
    }

    if (GET_OBJ_VNUM(obj) == OBJ_VEHCONTAINER) {
      send_to_char(ch, "You'd better drop %s if you want to free up your hands.\r\n", decapitalize_a_an(GET_OBJ_NAME(obj)));
      return;
    }

    if ((obj == cont) && !cyberdeck) {
      send_to_char(ch, "You attempt to fold %s into itself, but fail.\r\n", GET_OBJ_NAME(obj));
      return;
    }

    if (cyberdeck) {
      // Better messaging for parts.
      if (GET_OBJ_TYPE(obj) == ITEM_PART) {
        send_to_char(ch, "Parts aren't plug-and-play; you'll have to BUILD %s into your deck instead.\r\n", GET_OBJ_NAME(obj));
        return;
      }

      // You can only install programs, parts, and designs.
      if (GET_OBJ_TYPE(obj) != ITEM_PROGRAM && GET_OBJ_TYPE(obj) != ITEM_DECK_ACCESSORY && GET_OBJ_TYPE(obj) != ITEM_DESIGN) {
        send_to_char(ch, "You can't install %s into a cyberdeck.\r\n", GET_OBJ_NAME(obj));
        return;
      }

      // You can only install program designs into computers.
      if (GET_OBJ_TYPE(obj) == ITEM_DESIGN && GET_OBJ_TYPE(cont) != ITEM_DECK_ACCESSORY) {
        send_to_char("Program designs are just conceptual outlines and can't be installed into cyberdecks.\r\n", ch);
        return;
      }

      perform_put_cyberdeck(ch, obj, cont);
      return;
    }

    if (GET_OBJ_TYPE(cont) == ITEM_QUIVER) {
      if (GET_OBJ_VAL(cont, 1) == 0 && !(GET_OBJ_TYPE(obj) == ITEM_MISSILE && GET_OBJ_VAL(obj, 0) == 0))
        send_to_char(ch, "Only arrows may be placed in %s.\r\n", GET_OBJ_NAME(obj));
      else if (GET_OBJ_VAL(cont, 1) == 1 && !(GET_OBJ_TYPE(obj) == ITEM_MISSILE && GET_OBJ_VAL(obj, 0) == 1))
        send_to_char(ch, "Only bolts may be placed in %s.\r\n", GET_OBJ_NAME(obj));
      else if (GET_OBJ_VAL(cont, 1) == 2 && !(GET_OBJ_TYPE(obj) == ITEM_WEAPON && GET_OBJ_VAL(obj, 3) == TYPE_SHURIKEN))
        send_to_char(ch, "Only shurikens can be stored in %s.\r\n", GET_OBJ_NAME(obj));
      else if (GET_OBJ_VAL(cont, 1) == 3 && !(GET_OBJ_TYPE(obj) == ITEM_WEAPON && GET_OBJ_VAL(obj, 3) == TYPE_THROWING_KNIFE))
        send_to_char(ch, "%s is used to hold throwing knives only.\r\n", capitalize(GET_OBJ_NAME(obj)));
      else {
        perform_put(ch, obj, cont);
      }
      return;
    }

    perform_put(ch, obj, cont);
  } else {
    for (obj = ch->carrying; obj; obj = next_obj) {
      next_obj = obj->next_content;
      if (GET_OBJ_VNUM(obj) == OBJ_VEHCONTAINER)
        continue;
      if (obj != cont && CAN_SEE_OBJ(ch, obj) &&
          (obj_dotmode == FIND_ALL || isname(arg1, obj->text.keywords))) {
        found = 1;
        if (!cyberdeck) {
          perform_put(ch, obj, cont);
        } else if (GET_OBJ_TYPE(obj) != ITEM_PROGRAM)
          send_to_char(ch, "%s is not a program!\r\n", GET_OBJ_NAME(obj));
        else
          perform_put_cyberdeck(ch, obj, cont);
      }
    }
    if (!found) {
      if (obj_dotmode == FIND_ALL) {
        send_to_char(ch, "You don't seem to have anything in your inventory to %s in it.\r\n", (cyberdeck ? "install" : "put"));
      } else {
        send_to_char(ch, "You don't seem to have any %ss in your inventory.\r\n", arg1);
      }
    }
  }
}

bool can_take_obj(struct char_data * ch, struct obj_data * obj)
{
  if (IS_CARRYING_N(ch) >= CAN_CARRY_N(ch))
  {
    act("$p: you can't carry that many items.", FALSE, ch, obj, 0, TO_CHAR);
    return 0;
  }

  if ((IS_CARRYING_W(ch) + GET_OBJ_WEIGHT(obj)) > CAN_CARRY_W(ch)) {
    act("$p: you can't carry that much weight.", FALSE, ch, obj, 0, TO_CHAR);
    if (GET_OBJ_TYPE(obj) == ITEM_GUN_AMMO) {
      send_to_char("(You can still grab rounds out of it with ^WPOCKETS ADD <container>^n though!)\r\n", ch);
    }
    return 0;
  }

  if (!(CAN_WEAR(obj, ITEM_WEAR_TAKE)) && !ch->in_veh) {
    if (access_level(ch, LVL_PRESIDENT)) {
      act("You bypass the !TAKE flag on $p.", FALSE, ch, obj, 0, TO_CHAR);
    } else {
      act("$p: you can't take that!", FALSE, ch, obj, 0, TO_CHAR);
      return 0;
    }
  }

  if (obj->obj_flags.quest_id && obj->obj_flags.quest_id != GET_IDNUM(ch)) {
    if (access_level(ch, LVL_PRESIDENT)) {
      act("You bypass the quest flag on $p.", FALSE, ch, obj, 0, TO_CHAR);
    } else {
      act("$p is someone else's quest item.", FALSE, ch, obj, 0, TO_CHAR);
      return 0;
    }
  }

  return 1;
}

void get_check_money(struct char_data * ch, struct obj_data * obj, struct obj_data *from_obj)
{
  int zone;

  // Do nothing if it's not money.
  if (GET_OBJ_TYPE(obj) != ITEM_MONEY)
    return;

  // Find the zone it belongs to.
  for (zone = 0; zone <= top_of_zone_table; zone++)
    if (!zone_table[zone].connected && GET_OBJ_VNUM(obj) >= (zone_table[zone].number * 100) &&
        GET_OBJ_VNUM(obj) <= zone_table[zone].top)
      break;

  // Confirm that the zone is valid.
  if (zone <= top_of_zone_table) {
    act("$p dissolves in your hands!", FALSE, ch, obj, 0, TO_CHAR);
    snprintf(buf3, sizeof(buf3), "ERROR: Non-zone-contained item %ld obtained by player.", GET_OBJ_VNUM(obj));
    mudlog(buf3, ch, LOG_SYSLOG, TRUE);

    extract_obj(obj);
    return;
  }

  // Confirm that it has money on it.
  if (GET_ITEM_MONEY_VALUE(obj) <= (!GET_ITEM_MONEY_IS_CREDSTICK(obj) ? 0 : -1)) {
    act("$p dissolves in your hands!", FALSE, ch, obj, 0, TO_CHAR);
    snprintf(buf3, sizeof(buf3), "ERROR: Valueless money item %ld obtained by player.", GET_OBJ_VNUM(obj));
    mudlog(buf3, ch, LOG_SYSLOG, TRUE);

    extract_obj(obj);
    return;
  }

  // If it's paper money, handle it here.
  if (!GET_ITEM_MONEY_IS_CREDSTICK(obj)) {
    if (GET_ITEM_MONEY_VALUE(obj) > 1)
      send_to_char(ch, "There were %d nuyen.\r\n", GET_ITEM_MONEY_VALUE(obj));
    else
      send_to_char(ch, "There was 1 nuyen.\r\n");

    if (from_obj && (GET_OBJ_VNUM(from_obj) != OBJ_SPECIAL_PC_CORPSE && IS_OBJ_STAT(from_obj, ITEM_EXTRA_CORPSE))) {
      // Income from an NPC corpse is always tracked.
      gain_nuyen(ch, GET_ITEM_MONEY_VALUE(obj), NUYEN_INCOME_LOOTED_FROM_NPCS);
    } else {
      // Picking up money from a player corpse, or dropped money etc-- not a faucet, came from a PC.
      GET_NUYEN_RAW(ch) += GET_ITEM_MONEY_VALUE(obj);
    }

    extract_obj(obj);
    return;
  }

  // Credstick? Handle it here.
  else {
    // Income from an NPC corpse is always tracked. We don't add it to their cash level though-- credstick.
    if (from_obj && (GET_OBJ_VNUM(from_obj) != OBJ_SPECIAL_PC_CORPSE && IS_OBJ_STAT(from_obj, ITEM_EXTRA_CORPSE))) {
      if (IS_SENATOR(ch))
        send_to_char("(credstick from npc corpse)\r\n", ch);
      GET_NUYEN_INCOME_THIS_PLAY_SESSION(ch, NUYEN_INCOME_LOOTED_FROM_NPCS) += GET_ITEM_MONEY_VALUE(obj);
    }
    else {
      if (IS_SENATOR(ch))
        send_to_char("(credstick from pc corpse)\r\n", ch);
    }

    // We don't extract the credstick.
    return;
  }
}

void calc_weight(struct char_data *ch)
{
  struct obj_data *obj;
  int i=0;
  /* first reset the player carry weight*/
  IS_CARRYING_W(ch) = 0;

  // Go through worn equipment.
  for (i = 0; i < NUM_WEARS; i++)
    if (GET_EQ(ch, i))
      IS_CARRYING_W(ch) += GET_OBJ_WEIGHT(GET_EQ(ch, i));

  // Go through carried equipment.
  for (obj = ch->carrying; obj; obj = obj->next_content)
    IS_CARRYING_W(ch) += GET_OBJ_WEIGHT(obj);

  // Add cyberware per SR3 p300.
  for (obj = ch->cyberware; obj; obj = obj->next_content)
    if (GET_OBJ_VAL(obj, 0) == CYB_BONELACING)
      switch (GET_OBJ_VAL(obj, 3))
      {
      case BONE_KEVLAR:
      case BONE_PLASTIC:
        IS_CARRYING_W(ch) += 5;
        break;
      case BONE_ALUMINIUM:
        IS_CARRYING_W(ch) += 10;
        break;
      case BONE_TITANIUM:
      case BONE_CERAMIC:
        IS_CARRYING_W(ch) += 15;
        break;
      }

  // Add bullet pants.
  IS_CARRYING_W(ch) += get_bulletpants_weight(ch);
}

void perform_get_from_container(struct char_data * ch, struct obj_data * obj,
                                struct obj_data * cont, int mode)
{
  bool cyberdeck = FALSE, computer = FALSE;
  if (IS_WORKING(ch)) {
    send_to_char(TOOBUSY, ch);
    return;
  }
  if (GET_OBJ_TYPE(cont) == ITEM_CYBERDECK || GET_OBJ_TYPE(cont) == ITEM_CUSTOM_DECK)
    cyberdeck = TRUE;
  else if (GET_OBJ_TYPE(cont) == ITEM_DECK_ACCESSORY && GET_OBJ_VAL(cont, 0) == TYPE_COMPUTER)
    computer = TRUE;

  if (mode == FIND_OBJ_INV || ch == get_obj_possessor(obj) || can_take_obj(ch, obj))
  {
    if (IS_CARRYING_N(ch) >= CAN_CARRY_N(ch))
      act("$p: you can't hold any more items.", FALSE, ch, obj, 0, TO_CHAR);
    else {
      if ( (!IS_NPC(ch) && access_level(ch, LVL_BUILDER))
            || IS_OBJ_STAT(obj, ITEM_EXTRA_WIZLOAD)
            || (cont->obj_flags.extra_flags.IsSet(ITEM_EXTRA_CORPSE) && GET_OBJ_VAL(cont, 4))) {
        char *representation = generate_new_loggable_representation(obj);
        snprintf(buf, sizeof(buf), "%s gets from (%ld) %s [restring: %s]: %s",
                GET_CHAR_NAME(ch),
                GET_OBJ_VNUM( cont ), cont->text.name, cont->restring ? cont->restring : "none",
                representation);
        mudlog(buf, ch, IS_OBJ_STAT(obj, ITEM_EXTRA_WIZLOAD) ? LOG_WIZITEMLOG : LOG_CHEATLOG, TRUE);
        delete [] representation;
      }

      if (GET_OBJ_TYPE(cont) == ITEM_QUIVER)
        GET_OBJ_VAL(cont, 2) = MAX(0, GET_OBJ_VAL(cont, 2) - 1);
      snprintf(buf, sizeof(buf), "You %s $p from $P.", (cyberdeck || computer ? "uninstall" : "get"));

      if (computer) {
        if (ch->in_room) {
          for (struct char_data *vict = ch->in_room->people; vict; vict = vict->next_in_room) {
            if ((AFF_FLAGGED(vict, AFF_PROGRAM) || AFF_FLAGGED(vict, AFF_DESIGN)) && vict != ch) {
              send_to_char(ch, "You can't uninstall %s while someone is working on it.\r\n", GET_OBJ_NAME(obj));
              return;
            } else if (vict == ch && vict->char_specials.programming == obj) {
              send_to_char(ch, "You stop %sing %s.\r\n", AFF_FLAGGED(ch, AFF_PROGRAM) ? "programm" : "design", GET_OBJ_NAME(obj));
              STOP_WORKING(ch);
              break;
            }
          }
        } else {
          for (struct char_data *vict = ch->in_veh->people; vict; vict = vict->next_in_veh) {
            if ((AFF_FLAGGED(vict, AFF_PROGRAM) || AFF_FLAGGED(vict, AFF_DESIGN)) && vict != ch) {
              send_to_char(ch, "You can't uninstall %s while someone is working on it.\r\n", GET_OBJ_NAME(obj));
              return;
            } else if (vict == ch && vict->char_specials.programming == obj) {
              send_to_char(ch, "You stop %sing %s.\r\n", AFF_FLAGGED(ch, AFF_PROGRAM) ? "programm" : "design", GET_OBJ_NAME(obj));
              STOP_WORKING(ch);
              break;
            }
          }
        }
        if (GET_OBJ_TYPE(obj) == ITEM_PROGRAM)
          GET_CYBERDECK_TOTAL_STORAGE(cont) -= GET_OBJ_VAL(obj, 2);
        else
          GET_CYBERDECK_TOTAL_STORAGE(cont) -= GET_OBJ_VAL(obj, 6) + (GET_OBJ_VAL(obj, 6) / 10);
      }
      else if (cyberdeck) {
        if (GET_OBJ_TYPE(obj) == ITEM_PROGRAM && (GET_CYBERDECK_MPCP(cont) == 0 || GET_CYBERDECK_IS_INCOMPLETE(cont))) {
          display_cyberdeck_issues(ch, cont);
          return;
        }

        if (GET_OBJ_TYPE(obj) == ITEM_PROGRAM ||
            (GET_OBJ_TYPE(obj) == ITEM_DECK_ACCESSORY && GET_DECK_ACCESSORY_TYPE(obj) == TYPE_FILE))
          GET_OBJ_VAL(cont, 5) -= GET_DECK_ACCESSORY_FILE_SIZE(obj);

        if (GET_OBJ_TYPE(obj) == ITEM_PART) {
          if (GET_OBJ_VAL(obj, 0) == PART_STORAGE) {
            for (struct obj_data *k = cont->contains; k; k = k->next_content)
              if ((GET_OBJ_TYPE(k) == ITEM_DECK_ACCESSORY && GET_DECK_ACCESSORY_TYPE(k) == TYPE_FILE) ||
                  GET_OBJ_TYPE(k) == ITEM_PROGRAM) {
                send_to_char(ch, "You cannot uninstall %s while you have files installed.\r\n", GET_OBJ_NAME(obj));
                return;
              }
            GET_CYBERDECK_USED_STORAGE(cont) = GET_CYBERDECK_TOTAL_STORAGE(cont) = 0;
          }
          switch(GET_OBJ_VAL(obj, 0)) {
          case PART_MPCP:
              GET_PART_RATING(obj) = GET_CYBERDECK_MPCP(cont);
              // fall through
          case PART_ACTIVE:
          case PART_BOD:
          case PART_SENSOR:
          case PART_IO:
          case PART_MATRIX_INTERFACE:
            GET_CYBERDECK_IS_INCOMPLETE(cont) = 1;
          }
        }
      }

      act(buf, FALSE, ch, obj, cont, TO_CHAR);
      if (GET_OBJ_TYPE(cont) == ITEM_WORN)
        GET_OBJ_VAL(cont, GET_OBJ_TIMER(obj))++;
      if (!cyberdeck && !computer)
        act("$n gets $p from $P.", TRUE, ch, obj, cont, TO_ROOM);
      else
        act("$n uninstalls $p from $P.", TRUE, ch, obj, cont, TO_ROOM);

      {
        struct obj_data *was_in_obj = obj->in_obj;
        obj_from_obj(obj);
        obj_to_char(obj, ch);
        get_check_money(ch, obj, was_in_obj);
        obj = NULL;
      }

      if (cont->obj_flags.extra_flags.IsSet(ITEM_EXTRA_CORPSE) && GET_OBJ_VAL(cont, 4) && !cont->contains) {
        if (cont->in_room && ROOM_FLAGGED(cont->in_room, ROOM_CORPSE_SAVE_HACK)) {
          bool should_clear_flag = TRUE;

          // Iterate through items in room, making sure there are no other corpses.
          for (struct obj_data *tmp_obj = cont->in_room->contents; tmp_obj; tmp_obj = tmp_obj->next_content) {
            if (tmp_obj != cont && IS_OBJ_STAT(tmp_obj, ITEM_EXTRA_CORPSE) && GET_OBJ_BARRIER(tmp_obj) == PC_CORPSE_BARRIER) {
              should_clear_flag = FALSE;
              break;
            }
          }

          if (should_clear_flag) {
            snprintf(buf, sizeof(buf), "Removing storage flag from %s (%ld) due to no more player corpses being in it.",
                     GET_ROOM_NAME(cont->in_room),
                     GET_ROOM_VNUM(cont->in_room));
            mudlog(buf, NULL, LOG_SYSLOG, TRUE);

            // No more? Remove storage flag and save.
            cont->in_room->room_flags.RemoveBit(ROOM_CORPSE_SAVE_HACK);
            cont->in_room->room_flags.RemoveBit(ROOM_STORAGE);

            // Save the change.
            for (int counter = 0; counter <= top_of_zone_table; counter++) {
              if ((GET_ROOM_VNUM(cont->in_room) >= (zone_table[counter].number * 100))
                  && (GET_ROOM_VNUM(cont->in_room) <= (zone_table[counter].top)))
              {
                write_world_to_disk(zone_table[counter].number);
                break;
              }
            }
          }
        }

        act("$n takes the last of the items from $p.", TRUE, ch, cont, NULL, TO_ROOM);
        act("You take the last of the items from $p.", TRUE, ch, cont, NULL, TO_CHAR);
        extract_obj(cont);
      }
    }
  }
}


void get_from_container(struct char_data * ch, struct obj_data * cont,
                        char *arg, int mode, bool confirmed=FALSE)
{
  struct obj_data *obj, *next_obj;
  int obj_dotmode, found = 0;

  obj_dotmode = find_all_dots(arg, sizeof(arg));

  if (GET_OBJ_TYPE(cont) == ITEM_CYBERDECK || GET_OBJ_TYPE(cont) == ITEM_CUSTOM_DECK)
  {
    if (obj_dotmode == FIND_ALL || obj_dotmode == FIND_ALLDOT) {
      send_to_char("You can't uninstall more than one program at a time!\r\n", ch);
      return;
    }
    obj = get_obj_in_list_vis(ch, arg, cont->contains);
    if (!obj) {
      snprintf(buf, sizeof(buf), "There doesn't seem to be %s %s installed in $p.", AN(arg), arg);
      act(buf, FALSE, ch, cont, 0, TO_CHAR);
      return;
    }
    if (GET_OBJ_TYPE(obj) == ITEM_PART) {
      if (!confirmed) {
        send_to_char(ch, "It takes a while to reinstall parts once you've removed them! You'll have to do ^WUNINSTALL <part> <deck> CONFIRM^n to uninstall %s from %s.\r\n",
                      GET_OBJ_NAME(obj),
                      GET_OBJ_NAME(cont)
                    );
        return;
      }
    }
    perform_get_from_container(ch, obj, cont, mode);
    return;
  }
  if (GET_OBJ_TYPE(cont) == ITEM_DECK_ACCESSORY && GET_OBJ_VAL(cont, 0) == TYPE_COOKER)
  {
    if (GET_OBJ_VAL(cont, 9))
      send_to_char(ch, "You cannot remove a chip while it is being encoded.\r\n");
    else {
      obj = cont->contains;
      if (!obj)
        send_to_char(ch, "There's no chip in there to take out.\r\n");
      else
        perform_get_from_container(ch, obj, cont, mode);
    }
    return;
  }
  if ( IS_OBJ_STAT(cont, ITEM_EXTRA_CORPSE) )
  {
    if (GET_OBJ_VAL(cont, 4) == 1 && GET_OBJ_VAL(cont, 5) != GET_IDNUM(ch)
         && !IS_SENATOR(ch)) {
      send_to_char("You cannot loot this corpse.\r\n",ch);
      return;
/*    } else if (GET_OBJ_VAL(cont, 4) == 2 && !(PRF_FLAGGED(ch, PRF_PKER)
               || GET_IDNUM(ch) == GET_OBJ_VAL(cont, 5)) && !IS_SENATOR(ch)) {
      send_to_char("You cannot loot this corpse.\r\n", ch);
      return;*/
    } else {
      if (obj_dotmode == FIND_INDIV) {
        if (!(obj = get_obj_in_list_vis(ch, arg, cont->contains))) {
          snprintf(buf, sizeof(buf), "There doesn't seem to be %s %s in $p.", AN(arg), arg);
          act(buf, FALSE, ch, cont, 0, TO_CHAR);
        } else {
          perform_get_from_container(ch, obj, cont, mode);
          return;
        }
      } else {
        if (obj_dotmode == FIND_ALLDOT && !*arg) {
          send_to_char("Get all of what?\r\n", ch);
          return;
        }
        for (obj = cont->contains; obj; obj = next_obj) {
          next_obj = obj->next_content;
          if (CAN_SEE_OBJ(ch, obj) &&
              (obj_dotmode == FIND_ALL || isname(arg, obj->text.keywords))) {
            found = 1;
            perform_get_from_container(ch, obj, cont, mode);
          }
        }
        if (!found) {
          if (obj_dotmode == FIND_ALL)
            act("$p seems to be empty.", FALSE, ch, cont, 0, TO_CHAR);
          else {
            snprintf(buf, sizeof(buf), "You can't seem to find any %ss in $p.", arg);
            act(buf, FALSE, ch, cont, 0, TO_CHAR);
          }
        }
      }
      return;
    }
  }

  found = 0;

  if (IS_SET(GET_OBJ_VAL(cont, 1), CONT_CLOSED) && GET_OBJ_TYPE(cont) != ITEM_WORN && GET_OBJ_TYPE(cont) != ITEM_DECK_ACCESSORY)
    act("$p is closed.", FALSE, ch, cont, 0, TO_CHAR);
  else if (obj_dotmode == FIND_INDIV)
  {
    if (!(obj = get_obj_in_list_vis(ch, arg, cont->contains))) {
      snprintf(buf, sizeof(buf), "There doesn't seem to be %s %s in $p.", AN(arg), arg);
      act(buf, FALSE, ch, cont, 0, TO_CHAR);
    } else
      perform_get_from_container(ch, obj, cont, mode);
  } else
  {
    if (obj_dotmode == FIND_ALLDOT && !*arg) {
      send_to_char("Get all of what?\r\n", ch);
      return;
    }
    for (obj = cont->contains; obj; obj = next_obj) {
      next_obj = obj->next_content;
      if (CAN_SEE_OBJ(ch, obj) &&
          (obj_dotmode == FIND_ALL || isname(arg, obj->text.keywords))) {
        found = 1;
        perform_get_from_container(ch, obj, cont, mode);
      }
    }
    if (!found) {
      if (obj_dotmode == FIND_ALL)
        act("$p seems to be empty.", FALSE, ch, cont, 0, TO_CHAR);
      else {
        snprintf(buf, sizeof(buf), "You can't seem to find any %ss in $p.", arg);
        act(buf, FALSE, ch, cont, 0, TO_CHAR);
      }
    }
  }
}

int perform_get_from_room(struct char_data * ch, struct obj_data * obj, bool download)
{
  if (GET_OBJ_TYPE(obj) == ITEM_DECK_ACCESSORY) {
    switch (GET_OBJ_VAL(obj, 0)) {
      case TYPE_COMPUTER:
        for (struct char_data *vict = ch->in_veh ? ch->in_veh->people : ch->in_room->people; vict; vict = ch->in_veh ? vict->next_in_veh : vict->next_in_room)
          if (vict->char_specials.programming && vict->char_specials.programming->in_obj == obj) {
            if (vict == ch)
              send_to_char(ch, "You are using %s already.\r\n", GET_OBJ_NAME(obj));
            else
              act("$N seems to be using $p.", FALSE, ch, obj, vict, TO_CHAR);
            return FALSE;
          }
        break;
      case TYPE_COOKER:
        if (GET_OBJ_VAL(obj, 9)) {
          send_to_char(ch, "%s is in the middle of encoding a chip, leave it alone.\r\n", capitalize(GET_OBJ_NAME(obj)));
          return FALSE;
        }
        break;
    }
  }

  if (!can_take_obj(ch, obj)) {
    return FALSE;
  }

  if (GET_OBJ_TYPE(obj) == ITEM_WORKSHOP && GET_OBJ_VAL(obj, 1) > 1 && (GET_OBJ_VAL(obj, 2) || GET_OBJ_VAL(obj, 3))) {
    send_to_char(ch, "You may wish to pack %s up first.\r\n", GET_OBJ_NAME(obj));
    return FALSE;
  }


  if (GET_OBJ_TYPE(obj) == ITEM_WORKSHOP) {
    for (struct char_data *tmp = ch->in_veh ? ch->in_veh->people : ch->in_room->people; tmp; tmp = ch->in_veh ? tmp->next_in_veh : tmp->next_in_room)
       if (AFF_FLAGGED(tmp, AFF_PACKING)) {
         if (tmp == ch)
           send_to_char(ch, "You're already working on %s.\r\n", GET_OBJ_NAME(obj));
         else
           send_to_char(ch, "Someone is working on %s.\r\n", GET_OBJ_NAME(obj));
         return FALSE;
       }
  }

  if ( (!IS_NPC(ch) && access_level( ch, LVL_BUILDER ))
       || IS_OBJ_STAT( obj, ITEM_EXTRA_WIZLOAD) )
  {
    char *representation = generate_new_loggable_representation(obj);
    snprintf(buf, sizeof(buf), "%s gets from room: %s", GET_CHAR_NAME(ch), representation);
    mudlog(buf, ch, IS_OBJ_STAT(obj, ITEM_EXTRA_WIZLOAD) ? LOG_WIZITEMLOG : LOG_CHEATLOG, TRUE);
    delete [] representation;
  }

  obj_from_room(obj);
  obj_to_char(obj, ch);
  act("You get $p.", FALSE, ch, obj, 0, TO_CHAR);
  if (ch->in_veh) {
    snprintf(buf, sizeof(buf), "%s gets %s.\r\n", GET_NAME(ch), GET_OBJ_NAME(obj));
    send_to_veh(buf, ch->in_veh, ch, FALSE);
  } else
    act("$n gets $p.", FALSE, ch, obj, 0, TO_ROOM);
  get_check_money(ch, obj, NULL);
  affect_total(ch);
  return 1;
}

void get_from_room(struct char_data * ch, char *arg, bool download)
{
  struct obj_data *obj, *next_obj;
  int dotmode, found = 0;
  dotmode = find_all_dots(arg, sizeof(arg));

  if (dotmode == FIND_INDIV || download)
  {
    if (ch->in_veh)
      obj = get_obj_in_list_vis(ch, arg, ch->in_veh->contents);
    else
      obj = get_obj_in_list_vis(ch, arg, ch->in_room->contents);
    if (!obj) {
      // Attempt to pick up a vehicle.
      struct veh_data *veh;
      if ((veh = get_veh_list(arg, ch->in_veh ? ch->in_veh->carriedvehs : ch->in_room->vehicles, ch))) {
        int vehicle_weight = calculate_vehicle_weight(veh);
        rnum_t vehicle_storage_rnum = real_room(RM_PORTABLE_VEHICLE_STORAGE);

        if (vehicle_storage_rnum < 0) {
          send_to_char("Whoops-- looks like this system is offline!\r\n", ch);
          mudlog("SYSERR: Got negative rnum when looking up RM_PORTABLE_VEHICLE_STORAGE!", ch, LOG_SYSLOG, TRUE);
          return;
        }

        // No taking vehicles you can't carry.
        if (vehicle_weight + IS_CARRYING_W(ch) > CAN_CARRY_W(ch)) {
          send_to_char(ch, "Seeing as how it weighs %dkg, %s is too heavy for you to carry.\r\n", vehicle_weight, GET_VEH_NAME(veh));
          return;
        }

        // No taking vehicles that are moving.
        if (veh->cspeed != SPEED_OFF) {
          send_to_char(ch, "%s needs to be completely powered off before you can lift it.\r\n", capitalize(GET_VEH_NAME_NOFORMAT(veh)));
          return;
        }

        // No taking vehicles that are actively rigged.
        if (veh->rigger) {
          send_to_char(ch, "%s has someone in control of it, you'd better not.\r\n", capitalize(GET_VEH_NAME_NOFORMAT(veh)));
          return;
        }

        // No taking vehicles with people inside.
        if (veh->people) {
          send_to_char(ch, "%s has people inside it, you'd better not.\r\n", capitalize(GET_VEH_NAME_NOFORMAT(veh)));
          return;
        }

        // No taking vehicles with other vehicles inside.
        if (veh->carriedvehs) {
          send_to_char(ch, "%s has another vehicle inside it, there's no way you can carry that much!\r\n", capitalize(GET_VEH_NAME_NOFORMAT(veh)));
          return;
        }

        // No taking NPC vehicles or locked, non-destroyed vehicles that belong to someone else.
        if (!veh->owner || (veh->owner != GET_IDNUM(ch) && (veh->locked && veh->damage < VEH_DAM_THRESHOLD_DESTROYED))) {
          snprintf(buf, sizeof(buf), "%s's anti-theft measures beep loudly.\r\n", capitalize(GET_VEH_NAME_NOFORMAT(veh)));
          act(buf, FALSE, ch, 0, 0, TO_ROOM);
          send_to_char(buf, ch);
          return;
        }

        // You can carry it. Now comes the REAL bullshit. We bundle the vehicle up by transferring it to
        // a special room and giving you an item with enough values to point to it conclusively.

        // Initialize the vehicle container.
        struct obj_data *container = read_object(OBJ_VEHCONTAINER, VIRTUAL);

        if (!container) {
          send_to_char("Something went wrong! Don't worry, the vehicle is untouched. Please use the BUG command and tell us about what happened here.\r\n", ch);
          mudlog("SYSERR: Unable to read_object() on OBJ_VEHCONTAINER, did it not get built?", ch, LOG_SYSLOG, TRUE);
          return;
        }

        // Set the container's weight to match the vehicle.
        GET_OBJ_WEIGHT(container) = vehicle_weight;

        // Set the container's values to help us positively ID the vehicle.
        GET_VEHCONTAINER_VEH_VNUM(container)  = GET_VEH_VNUM(veh);
        GET_VEHCONTAINER_VEH_IDNUM(container) = veh->idnum;
        GET_VEHCONTAINER_VEH_OWNER(container) = veh->owner;

        // To avoid having to add another field to the pfiles_inv table, I've done something horrifyingly hacky and co-opted a value.
        // For vehicle containers, value 11 is the weight of the container.
        GET_VEHCONTAINER_WEIGHT(container) = GET_OBJ_WEIGHT(container);

        snprintf(buf, sizeof(buf), "^y%s^n (carried vehicle)", GET_VEH_NAME(veh));
        container->restring = str_dup(buf);

        // Give the object to the character.
        obj_to_char(container, ch);

        // Sanity check: If they're not carrying it for whatever reason, abort.
        if (container->carried_by != ch) {
          send_to_char("Whoops, something went wrong. Don't worry, the vehicle is untouched. Please use the BUG command and tell us what happened here.\r\n", ch);
          mudlog("SYSERR: Unable to give container object to character when picking up vehicle-- aborted!", ch, LOG_SYSLOG, TRUE);

          extract_obj(container);
          return;
        }

        // Move the vehicle to the storage room.
        veh_from_room(veh);
        veh_to_room(veh, &world[vehicle_storage_rnum]);

        // Finally, message the room.
        send_to_char(ch, "With a grunt, you lift %s.\r\n", GET_VEH_NAME(veh));
        snprintf(buf, sizeof(buf), "With a grunt, $n picks up %s.", GET_VEH_NAME(veh));
        act(buf, FALSE, ch, 0, 0, TO_ROOM);

        const char *owner = get_player_name(veh->owner);
        snprintf(buf, sizeof(buf), "%s (%ld) picked up vehicle %s (%ld, idnum %ld) belonging to %s (%ld).",
                 GET_CHAR_NAME(ch),
                 GET_IDNUM(ch),
                 GET_VEH_NAME(veh),
                 GET_VEH_VNUM(veh),
                 veh->idnum,
                 owner,
                 veh->owner
                );
        mudlog(buf, ch, LOG_CHEATLOG, TRUE);
        DELETE_ARRAY_IF_EXTANT(owner);

        playerDB.SaveChar(ch);
        save_vehicles(FALSE);
        return;
      }

      // Didn't find a vehicle, either.
      send_to_char(ch, "You don't see %s %s here.\r\n", AN(arg), arg);
    } else {
      if ( CAN_SEE_OBJ(ch, obj) ) {
        if ( IS_OBJ_STAT(obj, ITEM_EXTRA_CORPSE) && GET_OBJ_VAL(obj, 4) == 1
             && GET_OBJ_VAL(obj, 5) != GET_IDNUM(ch) && !IS_SENATOR(ch) )
          send_to_char("It's not yours chummer...better leave it be.\r\n",ch);
        else {
          perform_get_from_room(ch, obj, FALSE);
        }
        found = 1;
      }
    }
  } else
  {
    if (dotmode == FIND_ALLDOT && !*arg) {
      send_to_char("Get all of what?\r\n", ch);
      return;
    }

    if (ch->in_veh)
      obj = ch->in_veh->contents;
    else
      obj = ch->in_room->contents;

    for (;obj; obj = next_obj) {
      next_obj = obj->next_content;
      if (ch->in_veh && ch->vfront != obj->vfront)
        continue;
      if (CAN_SEE_OBJ(ch, obj) &&
          (dotmode == FIND_ALL || isname(arg, obj->text.keywords))) {
        found = 1;
        if ( IS_OBJ_STAT(obj, ITEM_EXTRA_CORPSE) && GET_OBJ_VAL(obj, 4) == 1
             && GET_OBJ_VAL(obj, 5) != GET_IDNUM(ch) && !access_level(ch, LVL_FIXER) )
          send_to_char("It's not yours chummer...better leave it be.\r\n",ch);
        else {
          perform_get_from_room(ch, obj, FALSE);
        }
      }
    }
    if (!found) {
      if (dotmode == FIND_ALL)
        send_to_char("There doesn't seem to be anything here.\r\n", ch);
      else {
        send_to_char(ch, "You don't see any %ss here.\r\n", arg);
      }
    }
  }
}

ACMD(do_get)
{
  char arg1[MAX_INPUT_LENGTH];
  char arg2[MAX_INPUT_LENGTH];

  int cont_dotmode, found = 0, mode, skill = 0, target = 0, kit = 0;
  sh_int bod = 0, load = 0, sig = 0;
  struct obj_data *cont, *obj, *temp, *shop = NULL;
  struct veh_data *veh = NULL;
  struct char_data *tmp_char;
  bool cyberdeck = FALSE, download = FALSE;

  if (IS_ASTRAL(ch)) {
    send_to_char("You cannot grasp physical objects.\r\n", ch);
    return;
  }

  if (subcmd == SCMD_UNINSTALL)
    cyberdeck = TRUE;
  const char *remainder = two_arguments(argument, arg1, arg2);

  if (IS_CARRYING_N(ch) >= CAN_CARRY_N(ch))
    send_to_char("Your arms are already full!\r\n", ch);
  else if (!*arg1) {
    send_to_char(ch, "%s what?\r\n", (cyberdeck ? "Uninstall" : (download ? "Download" : "Get")));
  } else if (!str_cmp(arg1, "tooth")) {
    for (cont = ch->cyberware; cont; cont = cont->next_content)
      if (GET_CYBERWARE_TYPE(cont) == CYB_TOOTHCOMPARTMENT)
        break;
    if (!cont)
      send_to_char("You don't have a tooth compartment.\r\n", ch);
    else if (!cont->contains)
      send_to_char("There's nothing in there.\r\n", ch);
    else if (success_test(GET_QUI(ch), 4) <= 0)
      send_to_char("You can't seem to get it out.\r\n", ch);
    else {
      obj = cont->contains;
      obj_from_obj(obj);
      obj_to_char(obj, ch);
      send_to_char(ch, "You remove %s from your tooth compartment.\r\n", GET_OBJ_NAME(obj));
      act("$n reaches into $s mouth and removes $p.", TRUE, ch, 0, obj, TO_ROOM);
    }
  } else if (!str_cmp(arg1, "finger")) {
    for (cont = ch->cyberware; cont; cont = cont->next_content)
      if (GET_CYBERWARE_TYPE(cont) == CYB_FINGERTIP)
        break;
    if (!cont)
      send_to_char("You don't have a fingertip compartment.\r\n", ch);
    else if (!cont->contains)
      send_to_char("There's nothing in there.\r\n", ch);
    else {
      obj = cont->contains;
      obj_from_obj(obj);
      obj_to_char(obj, ch);
      send_to_char(ch, "You remove %s from your fingertip compartment.\r\n", GET_OBJ_NAME(obj));
      act("$n removes $P from a fingertip compartment.", TRUE, ch, 0, obj, TO_ROOM);
    }
  } else if (!str_cmp(arg1, "body")) {
    for (cont = ch->cyberware; cont; cont = cont->next_content)
      if (GET_CYBERWARE_TYPE(cont) == CYB_BODYCOMPART)
        break;
    if (!cont)
      send_to_char("You don't have a body compartment.\r\n", ch);
    else if (!cont->contains)
      send_to_char("There's nothing in there.\r\n", ch);
    else {
      obj = cont->contains;
      obj_from_obj(obj);
      obj_to_char(obj, ch);
      send_to_char(ch, "You remove %s from your body compartment.\r\n", GET_OBJ_NAME(obj));
      act("$n removes $p from a body compartment.", TRUE, ch, 0, obj, TO_ROOM);
    }
  } else if (!*arg2 || download) {
    get_from_room(ch, arg1, download);
  } else {
    cont_dotmode = find_all_dots(arg2, sizeof(arg2));
    if (cont_dotmode == FIND_INDIV) {
      mode = generic_find(arg2, FIND_OBJ_EQUIP | FIND_OBJ_INV | FIND_OBJ_ROOM, ch, &tmp_char, &cont);
      if (!ch->in_veh || (ch->in_veh->flags.IsSet(VFLAG_WORKSHOP) && !ch->vfront))
        veh = get_veh_list(arg2, ch->in_veh ? ch->in_veh->carriedvehs : ch->in_room->vehicles, ch);
      if (cyberdeck && veh) {
        cont = NULL;
        if (!veh->owner || (veh->locked && veh->owner != GET_IDNUM(ch))) {
          snprintf(buf, sizeof(buf), "%s's anti-theft measures beep loudly.\r\n", capitalize(GET_VEH_NAME_NOFORMAT(veh)));
          act(buf, FALSE, ch, 0, 0, TO_ROOM);
          send_to_char(buf, ch);
          return;
        }
        if (veh->cspeed > SPEED_OFF) {
          send_to_char("It's moving a little fast...\r\n", ch);
          return;
        }
        for (found = 0; found < NUM_MODS; found++) {
          if (GET_MOD(veh, found)) {
            if (isname(arg1, GET_MOD(veh, found)->text.name)) {
              cont = GET_MOD(veh, found);
              break;
            }
          }
        }
        if (!cont && veh->mount) {
          for (obj = veh->mount; obj; obj = obj->next_content)
            if (isname(arg1, obj->text.name)) {
              cont = obj;
              cont_dotmode = 1;
              break;
            }
        }
        if (!cont) {
          send_to_char(ch, "There doesn't seem to be a %s installed on %s.\r\n", arg1, GET_VEH_NAME(veh));
          return;
        } else {
          if (!IS_NPC(ch)) {
            skill = get_br_skill_for_veh(veh);

            switch (GET_VEHICLE_MOD_TYPE(cont)) {
            case TYPE_ENGINECUST:
              target = 6;
              break;
            case TYPE_TURBOCHARGER:
              target = 2 + GET_OBJ_VAL(cont, 2);
              break;
            case TYPE_AUTONAV:
              target = 8 - veh->handling;
              break;
            case TYPE_CMC:
            case TYPE_DRIVEBYWIRE:
              target = 10 - veh->handling;
              break;
            case TYPE_ARMOR:
            case TYPE_CONCEALEDARMOR:
              target = (int)(GET_OBJ_VAL(cont, 2) / 3);
              break;
            case TYPE_ROLLBARS:
            case TYPE_TIRES:
            case TYPE_MISC:
              target = 3;
              break;
            default:
              target = 4;
              break;
            }
            skill = get_skill(ch, skill, target);
            target += modify_target(ch);
            kit = has_kit(ch, TYPE_VEHICLE);
            if ((shop = find_workshop(ch, TYPE_VEHICLE)))
              kit = GET_OBJ_VAL(shop, 0);
            if (!kit && !shop) {
              send_to_char("You don't have any tools here for working on vehicles.\r\n", ch);
              return;
            }
            if (kit < mod_types[GET_OBJ_VAL(cont, 0)].tools) {
              send_to_char(ch, "You don't have the right tools for that job.\r\n");
              return;
            } else if (mod_types[GET_OBJ_VAL(cont, 0)].tools == TYPE_KIT) {
              if (kit == TYPE_WORKSHOP)
                target--;
              else if (kit == TYPE_FACILITY)
                target -= 3;
            } else if (mod_types[GET_OBJ_VAL(cont, 0)].tools == TYPE_WORKSHOP && kit == TYPE_FACILITY)
              target--;
            if (GET_OBJ_VAL(cont, 0) == TYPE_ENGINECUST)
              veh->engine = 0;
            if (success_test(skill, target) < 1) {
              send_to_char(ch, "You can't figure out how to uninstall %s. \r\n", GET_OBJ_NAME(cont));
              return;
            }
          }
          if (GET_VEHICLE_MOD_TYPE(cont) == TYPE_MOUNT) {
            // Check to see if anyone is manning it.
            if (cont->worn_by) {
              send_to_char(ch, "Someone is manning %s.\r\n", GET_OBJ_NAME(cont));
              return;
            }
            // Make sure it's empty.
            if (cont->contains) {
              send_to_char(ch, "You'll have to remove the weapon from %s first.\r\n", GET_OBJ_NAME(cont));
              return;
            }
          }
          snprintf(buf, sizeof(buf), "$n goes to work on %s.", GET_VEH_NAME(veh));
          send_to_char(ch, "You go to work on %s and remove %s.\r\n", GET_VEH_NAME(veh), GET_OBJ_NAME(cont));
          act(buf, TRUE, ch, 0, 0, TO_ROOM);
          if (found == MOD_SEAT && cont->affected[0].modifier > 1) {
            cont->affected[0].modifier--;
            affect_veh(veh, cont->affected[0].location, -1);
            obj = read_object(GET_OBJ_VNUM(cont), VIRTUAL);
            cont = obj;
          } else if (cont_dotmode) {
            REMOVE_FROM_LIST(cont, veh->mount, next_content)
            switch (GET_OBJ_VAL(cont, 1)) {
              case 1:
                sig = 1;
                // fall through
              case 0:
                bod++;
                load = 10;
                break;
              case 3:
                sig = 1;
                // fall through
              case 2:
                bod += 2;
                load = 10;
                break;
              case 4:
                sig = 1;
                bod += 4;
                load = 100;
                break;
              case 5:
                sig = 1;
                bod += 2;
                load = 25;
                break;
            }
            veh->sig += sig;
            veh->usedload -= load;
          } else {
            if (GET_VEHICLE_MOD_TYPE(cont) == TYPE_AUTONAV) {
              veh->autonav -= GET_VEHICLE_MOD_RATING(cont);
            }
            veh->usedload -= GET_OBJ_VAL(cont, 1);
            GET_MOD(veh, found) = NULL;
            int rnum = real_vehicle(GET_VEH_VNUM(veh));
            if (rnum <= -1)
              send_to_char(ch, "Bro, your vehicle is _fucked_. Contact staff.\r\n");

            for (found = 0; found < MAX_OBJ_AFFECT; found++) {
              affect_veh(veh, cont->affected[found].location, -(cont->affected[found].modifier));

              switch (cont->affected[found].location) {
                case VAFF_SEN:
                  if (veh->sensor <= 0)
                    affect_veh(veh, VAFF_SEN, rnum >= 0 ? veh_proto[rnum].sensor : 0);
                  break;
                case VAFF_AUTO:
                  if (veh->autonav <= 0)
                    affect_veh(veh, VAFF_AUTO, rnum >= 0 ? veh_proto[rnum].autonav : 0);
                  break;
                case VAFF_PILOT:
                  if (veh->pilot <= 0)
                    affect_veh(veh, VAFF_PILOT, rnum >= 0 ? veh_proto[rnum].pilot : 0);
                  break;
              }
            }
          }
          obj_to_char(cont, ch);
          return;
        }
      } else if (!cont) {
        send_to_char(ch, "You don't have %s %s.\r\n", AN(arg2), arg2);
      } else if ((!cyberdeck && !(GET_OBJ_TYPE(cont) == ITEM_CONTAINER || GET_OBJ_TYPE(cont) == ITEM_KEYRING || GET_OBJ_TYPE(cont) ==
                                  ITEM_QUIVER || GET_OBJ_TYPE(cont) == ITEM_HOLSTER || GET_OBJ_TYPE(cont) ==
                                  ITEM_WORN)) || (cyberdeck && !(GET_OBJ_TYPE(cont) == ITEM_CYBERDECK ||
                                                                 GET_OBJ_TYPE(cont) == ITEM_CUSTOM_DECK ||
                                                                 GET_OBJ_TYPE(cont) == ITEM_DECK_ACCESSORY))) {
        snprintf(buf, sizeof(buf), "$p is not a %s.", (!cyberdeck ? "container" : "cyberdeck"));
        act(buf, FALSE, ch, cont, 0, TO_CHAR);

        if (access_level(ch, LVL_ADMIN) && !str_cmp(arg1, "force-all")) {
          send_to_char("Hoping you know what you're doing, you forcibly remove its contents anyways.\r\n", ch);
          struct obj_data *next;
          for (struct obj_data *contained = cont->contains; contained; contained = next) {
            next = contained->next_content;
            obj_from_obj(contained);
            obj_to_char(contained, ch);
            send_to_char(ch, "You retrieve %s from %s.\r\n", GET_OBJ_NAME(contained), GET_OBJ_NAME(cont));
          }
        }
      } else {
        get_from_container(ch, cont, arg1, mode, is_abbrev("confirm", remainder));
      }
    } else {
      if (cont_dotmode == FIND_ALLDOT && !*arg2) {
        send_to_char("Get from all of what?\r\n", ch);
        return;
      }
      for (cont = ch->carrying; cont; cont = cont->next_content)
        if (CAN_SEE_OBJ(ch, cont) &&
            (cont_dotmode == FIND_ALL || isname(arg2, cont->text.keywords))) {
          if ((!cyberdeck && (GET_OBJ_TYPE(cont) == ITEM_CONTAINER || GET_OBJ_TYPE(cont) == ITEM_KEYRING)) ||
              (cyberdeck && (GET_OBJ_TYPE(cont) == ITEM_CYBERDECK || GET_OBJ_TYPE(cont) == ITEM_CUSTOM_DECK))) {
            found = 1;
            get_from_container(ch, cont, arg1, FIND_OBJ_INV);
          } else if (cont_dotmode == FIND_ALLDOT) {
            found = 1;
            snprintf(buf, sizeof(buf), "$p is not a %s", (!cyberdeck ? "container" : "cyberdeck"));
            act(buf, FALSE, ch, cont, 0, TO_CHAR);
          }
        }
      FOR_ITEMS_AROUND_CH(ch, cont) {
        if (CAN_SEE_OBJ(ch, cont) &&
            (cont_dotmode == FIND_ALL || isname(arg2, cont->text.keywords))) {
          if (GET_OBJ_TYPE(cont) == ITEM_CONTAINER || GET_OBJ_TYPE(cont) == ITEM_KEYRING) {
            get_from_container(ch, cont, arg1, FIND_OBJ_ROOM);
            found = 1;
          } else if (cont_dotmode == FIND_ALLDOT) {
            snprintf(buf, sizeof(buf), "$p is not a %s", (!cyberdeck ? "container" : "cyberdeck"));
            act(buf, FALSE, ch, cont, 0, TO_CHAR);
            found = 1;
          }
        }
      }
      if (!found) {
        if (cont_dotmode == FIND_ALL) {
          send_to_char(ch, "You can't seem to find any %s.\r\n", (!cyberdeck ? "containers" : "cyberdeck"));
        } else {
          send_to_char(ch, "You can't seem to find any %ss here.\r\n", arg2);
        }
      }
    }
  }
}

void perform_drop_gold(struct char_data * ch, int amount, byte mode, struct room_data *random_donation_room)
{
  struct obj_data *obj;

  if (mode != SCMD_DROP)
  {
    send_to_char("You can't do that!\r\n", ch);
    return;
  } else if (amount < 1)
  {
    send_to_char("You can't drop less than one nuyen.\r\n", ch);
    return;
  } else if (amount > GET_NUYEN(ch))
  {
    send_to_char("You don't even have that much!\r\n", ch);
    return;
  }

  obj = read_object(OBJ_ROLL_OF_NUYEN, VIRTUAL);
  GET_OBJ_VAL(obj, 0) = amount;

  if ( !IS_NPC(ch) && (access_level(ch, LVL_BUILDER)
       || IS_NPC(ch)))
    obj->obj_flags.extra_flags.SetBit(ITEM_EXTRA_WIZLOAD);

  // Dropping money is not a sink.
  GET_NUYEN_RAW(ch) -= amount;
  act("You drop $p.", FALSE, ch, obj, 0, TO_CHAR);
  act("$n drops $p.", TRUE, ch, obj, 0, TO_ROOM);
  affect_total(ch);

  if (ch->in_veh)
  {
    obj_to_veh(obj, ch->in_veh);
    obj->vfront = ch->vfront;
  } else
    obj_to_room(obj, ch->in_room);

  if (IS_NPC(ch)
      || (!IS_NPC(ch) && access_level(ch, LVL_BUILDER)))
  {

    snprintf(buf, sizeof(buf), "%s drops: %d nuyen *", GET_CHAR_NAME(ch),
            amount);
    mudlog(buf, ch, LOG_CHEATLOG, TRUE);
  }

  return;
}

#define VANISH(mode) ((mode == SCMD_DONATE || mode == SCMD_JUNK) ? "  It vanishes into a recycling bin!" : "")

int perform_drop(struct char_data * ch, struct obj_data * obj, byte mode,
                 const char *sname, struct room_data *random_donation_room)
{
  int value;

  if (IS_OBJ_STAT(obj, ITEM_EXTRA_NODROP))
  {
    snprintf(buf, sizeof(buf), "You can't %s $p, it must be CURSED!", sname);
    act(buf, FALSE, ch, obj, 0, TO_CHAR);
    return 0;
  }
  if (IS_OBJ_STAT(obj, ITEM_EXTRA_KEPT) && !IS_SENATOR(ch)) {
    snprintf(buf, sizeof(buf), "You'll have to use the KEEP command on $p before you can %s it.", sname);
    act(buf, FALSE, ch, obj, 0, TO_CHAR);
    return 0;
  }
  if (obj == ch->char_specials.programming)
  {
    send_to_char(ch, "You can't %s something you are working on.\r\n", sname);
    return 0;
  }

  if (obj_contains_kept_items(obj) && !IS_SENATOR(ch)) {
    act("Action blocked: $p contains at least one kept item.", FALSE, ch, obj, 0, TO_CHAR);
    return 0;
  }

  if (mode == SCMD_DONATE) {
    if (IS_OBJ_STAT(obj, ITEM_EXTRA_WIZLOAD)) {
      act("You can't donate $p: It was given to you by staff.", FALSE, ch, obj, 0, TO_CHAR);
      return 0;
    }
    if (IS_OBJ_STAT(obj, ITEM_EXTRA_NOSELL) || IS_OBJ_STAT(obj, ITEM_EXTRA_NORENT)) {
      act("You can't donate $p: It's flagged !SELL or !RENT.", FALSE, ch, obj, 0, TO_CHAR);
      return 0;
    }
  }

  // Special handling: Vehicle containers.
  if (GET_OBJ_VNUM(obj) == OBJ_VEHCONTAINER) {
    if (mode != SCMD_DROP) {
      send_to_char(ch, "You can't %s vehicles.\r\n", sname);
      return 0;
    }

    if (ch->in_veh) {
      send_to_char("You'll have to step out of your current vehicle to do that.\r\n", ch);
      return 0;
    }

    // It'd be great if we could allow drones and bikes to be dropped anywhere not flagged !BIKE, but this
    // would cause issues with the current world-- the !bike flag is placed at entrances to zones, not
    // spread throughout the whole thing. People would just carry their bikes in, drop them, and do drivebys.
    if (!(ROOM_FLAGGED(ch->in_room, ROOM_ROAD) || ROOM_FLAGGED(ch->in_room, ROOM_GARAGE))) {
      send_to_char("You can only drop vehicles on roads or in garages.\r\n", ch);
      return 0;
    }

    // Find the veh storage room.
    rnum_t vehicle_storage_rnum = real_room(RM_PORTABLE_VEHICLE_STORAGE);
    if (vehicle_storage_rnum < 0) {
      send_to_char("Whoops-- looks like this system is offline!\r\n", ch);
      mudlog("SYSERR: Got negative rnum when looking up RM_PORTABLE_VEHICLE_STORAGE!", ch, LOG_SYSLOG, TRUE);
      return 0;
    }

    // Search it for our vehicle.
    for (struct veh_data *veh = world[vehicle_storage_rnum].vehicles; veh; veh = veh->next_veh) {
      if (GET_VEH_VNUM(veh) == GET_VEHCONTAINER_VEH_VNUM(obj)
          && veh->idnum == GET_VEHCONTAINER_VEH_IDNUM(obj)
          && veh->owner == GET_VEHCONTAINER_VEH_OWNER(obj))
      {
        // Found it! Proceed to drop.
        veh_from_room(veh);
        veh_to_room(veh, ch->in_room);
        send_to_char(ch, "You set %s down with a sigh of relief.\r\n", GET_VEH_NAME(veh));
        snprintf(buf, sizeof(buf), "$n sets %s down with a sigh of relief.", GET_VEH_NAME(veh));
        act(buf, FALSE, ch, 0, 0, TO_ROOM);

        const char *owner = get_player_name(veh->owner);
        snprintf(buf, sizeof(buf), "%s (%ld) dropped vehicle %s (%ld, idnum %ld) belonging to %s (%ld).",
                 GET_CHAR_NAME(ch),
                 GET_IDNUM(ch),
                 GET_VEH_NAME(veh),
                 GET_VEH_VNUM(veh),
                 veh->idnum,
                 owner,
                 veh->owner
                );
        mudlog(buf, ch, LOG_CHEATLOG, TRUE);
        DELETE_ARRAY_IF_EXTANT(owner);

        // Clear the values to prevent bug logging, then remove the object from inventory.
        GET_VEHCONTAINER_VEH_VNUM(obj) = GET_VEHCONTAINER_VEH_IDNUM(obj) = GET_VEHCONTAINER_VEH_OWNER(obj) = 0;
        extract_obj(obj);

        playerDB.SaveChar(ch);
        save_vehicles(FALSE);
        return 0;
      }
    }

    send_to_char(ch, "Error: we couldn't find a matching vehicle for %s! Please alert staff.\r\n", decapitalize_a_an(GET_OBJ_NAME(obj)));
    snprintf(buf, sizeof(buf), "SYSERR: Failed to find matching vehicle for container %s!", decapitalize_a_an(GET_OBJ_NAME(obj)));
    mudlog(buf, ch, LOG_SYSLOG, TRUE);
    return 0;
  }

  if (GET_OBJ_VNUM(obj) == OBJ_NEOPHYTE_SUBSIDY_CARD && GET_OBJ_VAL(obj, 1) > 0) {
    // TODO: Make it so you can use partial amounts for rent payments- this will suck with 1 nuyen left.
    send_to_char(ch, "You can't %s a subsidy card that still has nuyen on it!\r\n", sname);
    return 0;
  }

  else if (GET_OBJ_TYPE(obj) == ITEM_CUSTOM_DECK || GET_OBJ_TYPE(obj) == ITEM_CYBERDECK) {
    if (mode == SCMD_DONATE) {
      send_to_char("You can't donate cyberdecks!\r\n", ch);
      return 0;
    }

    if (obj->contains && mode == SCMD_JUNK) {
      send_to_char("You can't junk a cyberdeck that has components installed!\r\n", ch);
      return 0;
    }
  }

  else if ((mode == SCMD_DONATE || mode == SCMD_JUNK) && GET_OBJ_TYPE(obj) == ITEM_CONTAINER && obj->contains) {
    send_to_char(ch, "You'll have to empty %s before you can %s it.\r\n", decapitalize_a_an(GET_OBJ_NAME(obj)), sname);
    return 0;
  }

  if (ch->in_veh)
  {
    if (mode != SCMD_DONATE && mode != SCMD_JUNK) {
      if (ch->in_veh->usedload + GET_OBJ_WEIGHT(obj) > ch->in_veh->load) {
        send_to_char("There is too much in the vehicle already!\r\n", ch);
        return 0;
      }
      if (ch->vfront && ch->in_veh->seating[0] && ch->in_veh->usedload + GET_OBJ_WEIGHT(obj) > ch->in_veh->load / 10) {
        send_to_char("There is too much in the front of the vehicle!\r\n", ch);
        return 0;
      }
    }

    snprintf(buf, sizeof(buf), "%s %ss %s.%s\r\n", GET_NAME(ch), sname, GET_OBJ_NAME(obj), VANISH(mode));
    send_to_veh(buf, ch->in_veh, ch, FALSE);
  } else
  {
    snprintf(buf, sizeof(buf), "$n %ss $p.%s", sname, VANISH(mode));
    act(buf, TRUE, ch, obj, 0, TO_ROOM);
  }

  snprintf(buf, sizeof(buf), "You %s $p.%s", sname, VANISH(mode));
  act(buf, FALSE, ch, obj, 0, TO_CHAR);
  if (obj->in_obj)
    obj_from_obj(obj);
  else
    obj_from_char(obj);
  affect_total(ch);

  if ((mode == SCMD_DONATE) && IS_OBJ_STAT(obj, ITEM_EXTRA_NODONATE))
    mode = SCMD_JUNK;

  if ( (!IS_NPC(ch) && access_level( ch, LVL_BUILDER ))
       || IS_OBJ_STAT( obj, ITEM_EXTRA_WIZLOAD) )
  {
    char *representation = generate_new_loggable_representation(obj);
    snprintf(buf, sizeof(buf), "%s %ss: %s", GET_CHAR_NAME(ch),
            sname,
            representation);
    mudlog(buf, ch, IS_OBJ_STAT(obj, ITEM_EXTRA_WIZLOAD) ? LOG_WIZITEMLOG : LOG_CHEATLOG, TRUE);
    delete [] representation;
  }

  switch (mode)
  {
  case SCMD_DROP:
    if (ch->in_veh) {
      obj_to_veh(obj, ch->in_veh);
      obj->vfront = ch->vfront;
    } else
      obj_to_room(obj, ch->in_room);
    if (!IS_NPC(ch) && GET_QUEST(ch))
      check_quest_delivery(ch, obj);
    else if (AFF_FLAGGED(ch, AFF_GROUP) && ch->master &&
             !IS_NPC(ch->master) && GET_QUEST(ch->master))
      check_quest_delivery(ch->master, obj);
    return 0;
  case SCMD_DONATE:
    obj_to_room(obj, random_donation_room);
    if (random_donation_room->people)
      act("You notice $p exposed beneath the junk.", FALSE, random_donation_room->people, obj, 0, TO_ROOM);
    return 0;
  case SCMD_JUNK:
    value = MAX(1, MIN(200, GET_OBJ_COST(obj) >> 4));
    if (!IS_NPC(ch) && GET_QUEST(ch))
      check_quest_destroy(ch, obj);
    else if (AFF_FLAGGED(ch, AFF_GROUP) && ch->master &&
             !IS_NPC(ch->master) && GET_QUEST(ch->master))
      check_quest_destroy(ch->master, obj);
    extract_obj(obj);
    return value;
  default:
    log("SYSERR: Incorrect argument passed to perform_drop");
    break;
  }

  return 0;
}

ACMD(do_drop)
{
  extern rnum_t donation_room_1;
  extern rnum_t donation_room_2;  /* uncomment if needed! */
  extern rnum_t donation_room_3;  /* uncomment if needed! */


  if (IS_ASTRAL(ch)) {
    send_to_char("Astral projections can't touch things!\r\n", ch);
    return;
  }
  if (AFF_FLAGGED(ch, AFF_PILOT)) {
    send_to_char("While driving? Now that would be a good trick!\r\n", ch);
    return;
  }

  if (PLR_FLAGGED(ch, PLR_NOT_YET_AUTHED) && (subcmd == SCMD_DROP || subcmd == SCMD_DONATE)) {
    send_to_char(ch, "You cannot drop or donate items until you complete character generation.\r\n");
    return;
  } else if (IS_WORKING(ch)) {
    send_to_char(TOOBUSY, ch);
    return;
  }

  struct obj_data *obj, *next_obj;
  struct room_data *random_donation_room = NULL;
  byte mode = SCMD_DROP;
  int dotmode, amount = 0;
  const char *sname;

  switch (subcmd) {
  case SCMD_JUNK:
    sname = "junk";
    mode = SCMD_JUNK;
    break;
  case SCMD_DONATE:
    sname = "donate";
    mode = SCMD_DONATE;
    switch (number(0, 6)) {
    case 0:
      mode = SCMD_JUNK;
      break;
    case 6:
    case 1:
      random_donation_room = &world[real_room(donation_room_1)];
      break;
    case 5:
    case 2:
      random_donation_room = &world[real_room(donation_room_2)];
      break;
    case 4:
    case 3:
      random_donation_room = &world[real_room(donation_room_3)];
      break;
    }
    if (!random_donation_room && mode != SCMD_JUNK) {
      send_to_char("Sorry, you can't donate anything right now.\r\n", ch);
      return;
    }
    break;
  default:
    sname = "drop";
    break;
  }

  argument = one_argument(argument, arg);

  if (!*arg) {
    send_to_char(ch, "What do you want to %s?\r\n", sname);
    return;
  } else if (is_number(arg)) {
    amount = atoi(arg);
    argument = one_argument(argument, arg);
    if (!str_cmp("nuyen", arg))
      perform_drop_gold(ch, amount, mode, random_donation_room);
    else {
      /* code to drop multiple items.  anyone want to write it? -je */
      send_to_char("Sorry, you can't do that to more than one item at a time.\r\n", ch);
    }
    return;
  } else {
    dotmode = find_all_dots(arg, sizeof(arg));

    /* Can't junk or donate all */
    if ((dotmode == FIND_ALL) && (subcmd == SCMD_JUNK || subcmd == SCMD_DONATE)) {
      if (subcmd == SCMD_JUNK)
        send_to_char("Go to the dump if you want to junk EVERYTHING!\r\n", ch);
      else
        send_to_char("Go to the junkyard if you want to donate EVERYTHING!\r\n", ch);
      return;
    }
    if (dotmode == FIND_ALL) {
      if (!ch->carrying)
        send_to_char("You don't seem to be carrying anything.\r\n", ch);
      else
        for (obj = ch->carrying; obj; obj = next_obj) {
          next_obj = obj->next_content;
          amount += perform_drop(ch, obj, mode, sname, random_donation_room);
        }
    } else if (dotmode == FIND_ALLDOT) {
      if (!*arg) {
        send_to_char(ch, "What do you want to %s all of?\r\n", sname);
        return;
      }
      if (!(obj = get_obj_in_list_vis(ch, arg, ch->carrying))) {
        send_to_char(ch, "You don't seem to have any %ss in your inventory.\r\n", arg);
      }
      while (obj) {
        next_obj = get_obj_in_list_vis(ch, arg, obj->next_content);
        amount += perform_drop(ch, obj, mode, sname, random_donation_room);
        obj = next_obj;
      }
    } else {
      if (!(obj = get_obj_in_list_vis(ch, arg, ch->carrying))) {
        send_to_char(ch, "You don't seem to have %s %s in your inventory.\r\n", AN(arg), arg);
      } else
        amount += perform_drop(ch, obj, mode, sname, random_donation_room);
    }
  }
  if (amount && (subcmd == SCMD_JUNK) && !PLR_FLAGGED(ch, PLR_NOT_YET_AUTHED)) {
    send_to_char(ch, "You receive %d nuyen for recycling.\r\n", amount >> 4);
    gain_nuyen(ch, amount >> 4, NUYEN_INCOME_JUNKING);
  }
}

bool perform_give(struct char_data * ch, struct char_data * vict, struct obj_data * obj)
{
  if (IS_ASTRAL(vict))
  {
    act("What use would $E have for $p?!", FALSE, ch, obj, vict, TO_CHAR);
    return 0;
  }
  if (IS_OBJ_STAT(obj, ITEM_EXTRA_NODROP))
  {
    act("You can't let go of $p!!  Yeech!", FALSE, ch, obj, 0, TO_CHAR);
    return 0;
  }
  if (IS_OBJ_STAT(obj, ITEM_EXTRA_KEPT) && !IS_SENATOR(ch)) {
    act("You'll have to use the KEEP command on $p before you can give it away.", FALSE, ch, obj, 0, TO_CHAR);
    return 0;
  }
  if (obj_contains_kept_items(obj) && !IS_SENATOR(ch)) {
    act("Action blocked: $p contains at least one kept item.", FALSE, ch, obj, 0, TO_CHAR);
    return 0;
  }

  if (IS_CARRYING_N(vict) >= CAN_CARRY_N(vict))
  {
    act("$N seems to have $S hands full.", FALSE, ch, 0, vict, TO_CHAR);
    return 0;
  }
  if (GET_OBJ_WEIGHT(obj) + IS_CARRYING_W(vict) > CAN_CARRY_W(vict))
  {
    act("$E can't carry that much weight.", FALSE, ch, 0, vict, TO_CHAR);
    return 0;
  }
  if (obj == ch->char_specials.programming)
  {
    send_to_char(ch, "You cannot give away something you are working on.\r\n");
    return 0;
  }
  if (GET_OBJ_VNUM(obj) == OBJ_VEHCONTAINER) {
    if (IS_NPC(vict)) {
      send_to_char("You can't give NPCs vehicles.\r\n", ch);
      return 0;
    }

    else {
      const char *owner = get_player_name(GET_VEHCONTAINER_VEH_OWNER(obj));
      snprintf(buf, sizeof(buf), "%s (%ld) gave veh-container %s (%d, idnum %d), belonging to %s (%d), to %s (%ld).",
               GET_CHAR_NAME(ch),
               GET_IDNUM(ch),
               GET_OBJ_NAME(obj),
               GET_VEHCONTAINER_VEH_VNUM(obj),
               GET_VEHCONTAINER_VEH_IDNUM(obj),
               owner,
               GET_VEHCONTAINER_VEH_OWNER(obj),
               GET_CHAR_NAME(vict),
               GET_IDNUM(vict)
              );
      mudlog(buf, ch, LOG_CHEATLOG, TRUE);
      DELETE_ARRAY_IF_EXTANT(owner);
    }
  }

  obj_from_char(obj);
  obj_to_char(obj, vict);
  act("You give $p to $N.", FALSE, ch, obj, vict, TO_CHAR);
  act("$n gives you $p.", FALSE, ch, obj, vict, TO_VICT);
  if (ch->in_veh)
  {
    snprintf(buf, sizeof(buf), "%s gives %s to %s.\r\n", GET_NAME(ch), GET_OBJ_NAME(obj), GET_NAME(vict));
    send_to_veh(buf, ch->in_veh, ch, vict, FALSE);
  }
  act("$n gives $p to $N.", TRUE, ch, obj, vict, TO_NOTVICT);

  if ( (!IS_NPC(ch) && access_level( ch, LVL_BUILDER ))
       || IS_OBJ_STAT( obj, ITEM_EXTRA_WIZLOAD) )
  {
    // Default/preliminary logging message; this is appended to where necessary.
    char *representation = generate_new_loggable_representation(obj);
    snprintf(buf, sizeof(buf), "%s gives %s: %s", GET_CHAR_NAME(ch), GET_CHAR_NAME(vict), representation);
    mudlog(buf, ch, IS_OBJ_STAT(obj, ITEM_EXTRA_WIZLOAD) ? LOG_WIZITEMLOG : LOG_CHEATLOG, TRUE);
    delete [] representation;
  }

  if (!IS_NPC(ch) && IS_NPC(vict)) {
    // Group quest reward.
    if (AFF_FLAGGED(ch, AFF_GROUP) && ch->master && !IS_NPC(ch->master) && IS_NPC(vict) && GET_QUEST(ch->master)) {
      if (check_quest_delivery(ch->master, vict, obj)) {
        act("$n nods slightly to $N and tucks $p away.", TRUE, vict, obj, ch, TO_ROOM);
        extract_obj(obj);
      }
    }
    // Individual quest reward.
    else if (GET_QUEST(ch) && check_quest_delivery(ch, vict, obj)) {
      act("$n nods slightly to $N and tucks $p away.", TRUE, vict, obj, ch, TO_ROOM);
      extract_obj(obj);
    }
    // No quest found.
    else {
      if (GET_MOB_SPEC(vict) || GET_MOB_SPEC2(vict)) {
        // These specs handle objects, so don't mess with them.
        if (GET_MOB_SPEC(vict) == fence || GET_MOB_SPEC(vict) == hacker || GET_MOB_SPEC(vict) == fixer || GET_MOB_SPEC(vict) == mageskill_herbie)
          return 1;
        if (GET_MOB_SPEC2(vict) == fence || GET_MOB_SPEC2(vict) == hacker || GET_MOB_SPEC2(vict) == fixer || GET_MOB_SPEC2(vict) == mageskill_herbie)
          return 1;
      }

      act("$n glances at $p, then lets it fall from $s hand.", TRUE, vict, obj, 0, TO_ROOM);
      obj_from_char(obj);
      if (vict->in_room)
        obj_to_room(obj, vict->in_room);
      else
        obj_to_veh(obj, vict->in_veh);
    }
  }

  return 1;
}

/* utility function for give */
struct char_data *give_find_vict(struct char_data * ch, char *arg)
{
  SPECIAL(fixer);
  struct char_data *vict;

  if (!*arg)
  {
    send_to_char("To who?\r\n", ch);
    return NULL;
  } else if (!(vict = get_char_room_vis(ch, arg)))
  {
    send_to_char(ch, "You don't see anyone named '%s' here.\r\n", arg);
    return NULL;
  } else if (vict == ch)
  {
    send_to_char("What's the point of giving it to yourself?\r\n", ch);
    return NULL;
  } else if (IS_NPC(vict) && GET_MOB_SPEC(vict) && GET_MOB_SPEC(vict) == fixer)
  {
    act("Do you really want to give $M stuff for free?! (Use the 'repair' command here.)",
        FALSE, ch, 0, vict, TO_CHAR);
    return NULL;
  } else
    return vict;
}

void perform_give_gold(struct char_data *ch, struct char_data *vict, int amount)
{
  if (amount <= 0)
  {
    send_to_char("Heh heh heh ... we are jolly funny today, eh?\r\n", ch);
    return;
  }
  if (IS_ASTRAL(vict))
  {
    send_to_char("Yeah, like astrals need nuyen.\r\n", ch);
    return;
  }
  if ((GET_NUYEN(ch) < amount) && (IS_NPC(ch) || (!access_level(ch, LVL_VICEPRES))))
  {
    send_to_char("You don't have that much!\r\n", ch);
    return;
  }
  if (IS_SENATOR(ch) && !access_level(ch, LVL_FREEZE) && !IS_SENATOR(vict))
  {
    send_to_char("You're not a high-enough level of staffer to do that.\r\n", ch);
    return;
  }
  send_to_char(OK, ch);
  snprintf(buf, sizeof(buf), "$n gives you %d nuyen.", amount);
  act(buf, FALSE, ch, 0, vict, TO_VICT);
  if (ch->in_veh)
  {
    snprintf(buf, sizeof(buf), "%s gives some nuyen to %s.", GET_NAME(ch), GET_NAME(vict));
    send_to_veh(buf, ch->in_veh, ch, vict, FALSE);
  } else
  {
    snprintf(buf, sizeof(buf), "$n gives some nuyen to $N.");
    act(buf, TRUE, ch, 0, vict, TO_NOTVICT);
  }

  // Giving nuyen. NPCs and non-staff PCs lose the amount they give, staff do not.
  if (IS_NPC(ch) || !access_level(ch, LVL_VICEPRES))
    GET_NUYEN_RAW(ch) -= amount;
  GET_NUYEN_RAW(vict) += amount;

  snprintf(buf, sizeof(buf), "%s gives %s: %d nuyen *",
          GET_CHAR_NAME(ch), GET_CHAR_NAME(vict), amount);
  mudlog(buf, ch, LOG_GRIDLOG, TRUE);
}

ACMD(do_give)
{
  int amount, dotmode;
  struct char_data *vict;
  struct obj_data *obj, *next_obj;

  argument = one_argument(argument, arg);

  if (IS_ASTRAL(ch)) {
    send_to_char("Astral projections can't touch anything.\r\n", ch);
    return;
  }
  if (PLR_FLAGGED(ch, PLR_NOT_YET_AUTHED)) {
    send_to_char("Sorry, you can't do that during character generation.\r\n", ch);
    return;
  }
  if (IS_WORKING(ch)) {
    send_to_char(TOOBUSY, ch);
    return;
  }
  if (!*arg)
    send_to_char("Give what to who?\r\n", ch);
  else if (is_number(arg)) {
    amount = atoi(arg);
    argument = one_argument(argument, arg);
    if (!str_cmp("nuyen", arg)) {
      argument = one_argument(argument, arg);

      if (ch->in_veh) {
        if (!(vict = get_char_veh(ch, arg, ch->in_veh)) || IS_IGNORING(vict, is_blocking_ic_interaction_from, ch)) {
          send_to_char(ch, "You don't see anyone named '%s' here.\r\n", arg);
        } else {
          perform_give_gold(ch, vict, amount);
        }
        return;
      }

      if (!(vict = give_find_vict(ch, arg)) || IS_IGNORING(vict, is_blocking_ic_interaction_from, ch)) {
        send_to_char(ch, "You don't see anyone named '%s' here.\r\n", arg);
      } else {
        perform_give_gold(ch, vict, amount);
      }
      return;

    } else {
      /* code to give multiple items.  anyone want to write it? -je */
      send_to_char("You can't give more than one item at a time.\r\n", ch);
      return;
    }
  } else {
    one_argument(argument, buf1);
    if (ch->in_veh) {
      if (!(vict = get_char_veh(ch, buf1, ch->in_veh))) {
        send_to_char(ch, "You don't see anyone named '%s' here.\r\n", buf1);
        return;
      }
    } else if (!(vict = give_find_vict(ch, buf1)))
      return;

    if (IS_IGNORING(vict, is_blocking_ic_interaction_from, ch)) {
      send_to_char(ch, "You don't see anyone named '%s' here.\r\n", buf1);
      return;
    }

    dotmode = find_all_dots(arg, sizeof(arg));
    if (dotmode == FIND_INDIV) {
      if (!(obj = get_obj_in_list_vis(ch, arg, ch->carrying))) {
        send_to_char(ch, "You don't seem to have %s %s in your inventory.\r\n", AN(arg), arg);
      } else
        perform_give(ch, vict, obj);
    } else {
      if (dotmode == FIND_ALLDOT && !*arg) {
        send_to_char("All of what?\r\n", ch);
        return;
      }
      if (!ch->carrying)
        send_to_char("You don't seem to be holding anything.\r\n", ch);
      else
        for (obj = ch->carrying; obj; obj = next_obj) {
          next_obj = obj->next_content;
          if (CAN_SEE_OBJ(ch, obj) &&
              ((dotmode == FIND_ALL || isname(arg, obj->text.keywords))))
            perform_give(ch, vict, obj);
        }
    }
  }
}

/* Everything from here down is what was formerly act.obj2.c */

void weight_change_object(struct obj_data * obj, float weight)
{
  struct obj_data *tmp_obj = NULL;
  struct char_data *tmp_ch = NULL;
  struct veh_data *tmp_veh = NULL;
  int worn_on = -1;

  // Remove it from its container (subtracting its current weight from the container's values).
  if ((tmp_ch = obj->carried_by))
    obj_from_char(obj);
  else if ((tmp_obj = obj->in_obj))
    obj_from_obj(obj);
  else if ((tmp_veh = obj->in_veh))
    obj_from_room(obj);
  else if (obj->worn_by && (worn_on = obj->worn_on) >= 0)
    unequip_char((tmp_ch = obj->worn_by), obj->worn_on, TRUE);

  // If none of the above are true, then this object is either in a room or is being juggled by the code somewhere (ex: zoneloading). Either way, no parent containers need updating.

  // Rectify weights so that the object's weight can never be negative.
  GET_OBJ_WEIGHT(obj) = MAX(0, GET_OBJ_WEIGHT(obj) + weight);

  // Return it to its container, re-adding its weight.
  if (tmp_ch) {
    if (worn_on >= 0)
      equip_char(tmp_ch, obj, worn_on);
    else
      obj_to_char(obj, tmp_ch);
  } else if (tmp_obj)
    obj_to_obj(obj, tmp_obj);
  else if (tmp_veh)
    obj_to_veh(obj, tmp_veh);
}

void name_from_drinkcon(struct obj_data *obj)
{
  char token[80], *temp; // hopefully this will be enough, hehehe
  extern struct obj_data *obj_proto;

  temp = get_token(obj->text.keywords, token);

  buf2[0] = '\0'; // so strcats will start at the beginning

  int i = 0;
  while (*token && strcasecmp(token, drinks[GET_OBJ_VAL(obj, 2)]))
  {
    strcat(buf2, token);
    strcat(buf2, " ");
    temp = get_token(temp, token);
    ++i;
  }

  // we do this in case there's only one word in the name list and it
  // is the name of the liquid
  if (i < 1)
    return;

  // now, we copy the remaining string onto the end of buf2
  if (temp)
    strcat(buf2, temp);

  buf2[strlen(buf2)] = '\0'; // remove the trailing space

  // only delete the object name if this object has a unique name
  if (GET_OBJ_RNUM(obj) < 0 || obj->text.keywords != obj_proto[GET_OBJ_RNUM(obj)].text.keywords)
    DELETE_AND_NULL_ARRAY(obj->text.keywords);
  // otherwise it just gets pointed to the new namelist
  obj->text.keywords = str_dup(buf2);
}

void name_to_drinkcon(struct obj_data *obj, int type)
{
  char *new_name;

  if (!obj || (GET_OBJ_TYPE(obj) != ITEM_DRINKCON && GET_OBJ_TYPE(obj) != ITEM_FOUNTAIN))
    return;

  size_t length = strlen(obj->text.keywords) + strlen(drinknames[type]) + 2;
  new_name = new char[length];
  snprintf(new_name, length, "%s %s", obj->text.keywords, drinknames[type]);

  if (GET_OBJ_RNUM(obj) == NOTHING ||
      obj->text.keywords != obj_proto[GET_OBJ_RNUM(obj)].text.keywords)
    DELETE_AND_NULL_ARRAY(obj->text.keywords);

  obj->text.keywords = new_name;
}

ACMD(do_drink)
{
  struct obj_data *temp;
  int amount, on_ground = 0;
  float weight;

  one_argument(argument, arg);

  if (IS_ASTRAL(ch)) {
    send_to_char("Astral forms can't retain fluid.\r\n", ch);
    return;
  }

  if (!*arg) {
    send_to_char("Drink from what?\r\n", ch);
    return;
  }
  if (!(temp = get_obj_in_list_vis(ch, arg, ch->carrying))) {
    if (!ch->in_room || !(temp = get_obj_in_list_vis(ch, arg, ch->in_room->contents))) {
      send_to_char(ch, "You don't see anything named '%s' here.\r\n", arg);
      return;
    } else
      on_ground = 1;
  }
  if ((GET_OBJ_TYPE(temp) != ITEM_DRINKCON) &&
      (GET_OBJ_TYPE(temp) != ITEM_FOUNTAIN)) {
    send_to_char(ch, "You can't drink from %s.\r\n", GET_OBJ_NAME(temp));
    return;
  }
  if (on_ground && (GET_OBJ_TYPE(temp) == ITEM_DRINKCON)) {
    send_to_char(ch, "You have to be holding %s to drink from it.\r\n", GET_OBJ_NAME(temp));
    return;
  }
  if ((GET_COND(ch, COND_DRUNK) > MAX_DRUNK)) {
    send_to_char("You can't seem to get close enough to your mouth.\r\n", ch);
    act("$n tries to drink but misses $s mouth!", TRUE, ch, 0, 0, TO_ROOM);
    return;
  }
#ifdef ENABLE_HUNGER
  if ((GET_COND(ch, COND_FULL) > MAX_FULLNESS) && (GET_COND(ch, COND_THIRST) > MIN_QUENCHED)) {
    send_to_char("Your stomach can't contain anymore!\r\n", ch);
    return;
  }
#endif
  if (!GET_OBJ_VAL(temp, 1)) {
    send_to_char("It's empty.\r\n", ch);
    return;
  }
  if (subcmd == SCMD_DRINK) {
    snprintf(buf, sizeof(buf), "$n drinks %s from $p.", drinknames[GET_OBJ_VAL(temp, 2)]);
    act(buf, TRUE, ch, temp, 0, TO_ROOM);

    send_to_char(ch, "You drink the %s.\r\n", drinknames[GET_OBJ_VAL(temp, 2)]);
    amount = number(3, 10);
  } else {
    act("$n sips from $p.", TRUE, ch, temp, 0, TO_ROOM);
    send_to_char(ch, "It tastes like %s.\r\n", drinknames[GET_OBJ_VAL(temp, 2)]);
    amount = 1;
  }

  amount = MIN(amount, GET_OBJ_VAL(temp, 1));

  /* You can't subtract more than the object weighs */
  weight = (float)(MIN(amount * 100, (int)(GET_OBJ_WEIGHT(temp) * 100)) / 100);

  weight_change_object(temp, -weight);  /* Subtract amount */

  gain_condition(ch, COND_DRUNK,
                 (int) ((int) drink_aff[GET_OBJ_VAL(temp, 2)][COND_DRUNK] * amount) / 4);

#ifdef ENABLE_HUNGER
  gain_condition(ch, COND_FULL,
                 (int) ((int) drink_aff[GET_OBJ_VAL(temp, 2)][COND_FULL] * amount) / 4);

  gain_condition(ch, COND_THIRST,
                 (int) ((int) drink_aff[GET_OBJ_VAL(temp, 2)][COND_THIRST] * amount) / 4);
#endif

  if (GET_COND(ch, COND_DRUNK) > MAX_DRUNK)
    send_to_char("You feel drunk.\r\n", ch);

#ifdef ENABLE_HUNGER
  if (GET_COND(ch, COND_THIRST) > MAX_QUENCHED)
    send_to_char("You don't feel thirsty any more.\r\n", ch);

  if (GET_COND(ch, COND_FULL) > MAX_FULLNESS)
    send_to_char("You are full.\r\n", ch);
#endif

  /* empty the container, and no longer poison. */
  GET_OBJ_VAL(temp, 1) -= amount;
  if (!GET_OBJ_VAL(temp, 1)) {  /* The last bit */
    //name_from_drinkcon(temp); // do this first
    GET_OBJ_VAL(temp, 2) = 0;
  }
  return;
}

ACMD(do_eat)
{
  SPECIAL(anticoagulant);
  struct obj_data *food, *cyber;
  int amount;

  if (IS_ASTRAL(ch)) {
    send_to_char("Eat?!  You don't even have a stomach!.\r\n", ch);
    return;
  }
  one_argument(argument, arg);

  if (!*arg) {
    send_to_char("Eat what?\r\n", ch);
    return;
  }
  if (!(food = get_obj_in_list_vis(ch, arg, ch->carrying))) {
    send_to_char(ch, "You don't seem to have %s %s in your inventory.\r\n", AN(arg), arg);
    return;
  }
  if (subcmd == SCMD_TASTE && ((GET_OBJ_TYPE(food) == ITEM_DRINKCON) ||
                               (GET_OBJ_TYPE(food) == ITEM_FOUNTAIN))) {
    do_drink(ch, argument, 0, SCMD_SIP);
    return;
  }
  if ((GET_OBJ_TYPE(food) != ITEM_FOOD) && !access_level(ch, LVL_ADMIN)) {
    send_to_char(ch, "You can't eat %s!\r\n", GET_OBJ_NAME(food));
    return;
  }
#ifdef ENABLE_HUNGER
  if (GET_COND(ch, COND_FULL) > MAX_FULLNESS) {/* Stomach full */
    act("You are too full to eat more!", FALSE, ch, 0, 0, TO_CHAR);
    return;
  }
#endif

  if (subcmd == SCMD_EAT) {
    act("You eat $p.", FALSE, ch, food, 0, TO_CHAR);
    act("$n eats $p.", TRUE, ch, food, 0, TO_ROOM);
    if (GET_OBJ_SPEC(food) && GET_OBJ_SPEC(food) == anticoagulant) {
      for (cyber = ch->bioware; cyber; cyber = cyber->next_content)
        if (GET_OBJ_VAL(cyber, 0) == BIO_PLATELETFACTORY) {
          GET_OBJ_VAL(cyber, 5) = 36;
          GET_OBJ_VAL(cyber, 6) = 0;
          send_to_char("You relax as your platelet factory calms down.\r\n", ch);
          break;
        }
    }
  } else {
    act("You nibble a little bit of the $o.", FALSE, ch, food, 0, TO_CHAR);
    act("$n tastes a little bit of $p.", TRUE, ch, food, 0, TO_ROOM);
  }

  amount = (subcmd == SCMD_EAT ? GET_OBJ_VAL(food, 0) : 1);

  gain_condition(ch, COND_FULL, amount);

  if (GET_COND(ch, COND_FULL) > 20)
    act("You are full.", FALSE, ch, 0, 0, TO_CHAR);

  if (subcmd == SCMD_EAT)
    extract_obj(food);
  else {
    if (!(--GET_OBJ_VAL(food, 0))) {
      send_to_char("There's nothing left now.\r\n", ch);
      extract_obj(food);
    }
  }
}

ACMD(do_pour)
{
  char arg1[MAX_INPUT_LENGTH];
  char arg2[MAX_INPUT_LENGTH];
  struct obj_data *from_obj = NULL, *to_obj = NULL;
  int amount;

  two_arguments(argument, arg1, arg2);

  if (subcmd == SCMD_POUR) {
    if (!*arg1) {               /* No arguments */
      act("What do you want to pour from?", FALSE, ch, 0, 0, TO_CHAR);
      return;
    }
    if (!(from_obj = get_obj_in_list_vis(ch, arg1, ch->carrying))) {
      send_to_char(ch, "You don't see anything named '%s' here.\r\n", arg1);
      return;
    }
    if (GET_OBJ_TYPE(from_obj) != ITEM_DRINKCON) {
      act("You can't pour from $p.", FALSE, ch, from_obj, 0, TO_CHAR);
      return;
    }
  }
  if (subcmd == SCMD_FILL) {
    if (!*arg1) {               /* no arguments */
      send_to_char("What do you want to fill?  And what are you filling it from?\r\n", ch);
      return;
    }
    if (!(to_obj = get_obj_in_list_vis(ch, arg1, ch->carrying))) {
      send_to_char(ch, "You don't see anything named '%s' here.\r\n", arg1);
      return;
    }
    if (GET_OBJ_TYPE(to_obj) != ITEM_DRINKCON) {
      act("You can't fill $p!", FALSE, ch, to_obj, 0, TO_CHAR);
      return;
    }
    if (!*arg2) {               /* no 2nd argument */
      act("What do you want to fill $p from?", FALSE, ch, to_obj, 0, TO_CHAR);
      return;
    }
    if (!(from_obj = get_obj_in_list_vis(ch, arg2, ch->in_room->contents))) {
      send_to_char(ch, "There doesn't seem to be %s %s here.\r\n", AN(arg2), arg2);
      return;
    }
    if (GET_OBJ_TYPE(from_obj) != ITEM_FOUNTAIN) {
      act("You can't fill something from $p.", FALSE, ch, from_obj, 0, TO_CHAR);
      return;
    }
  }
  if (GET_OBJ_VAL(from_obj, 1) == 0) {
    act("The $p is empty.", FALSE, ch, from_obj, 0, TO_CHAR);
    return;
  }
  if (subcmd == SCMD_POUR) {    /* pour */
    if (!*arg2) {
      act("Where do you want it?  Out or in what?", FALSE, ch, 0, 0, TO_CHAR);
      return;
    }
    if (!str_cmp(arg2, "out")) {
      act("$n empties $p.", TRUE, ch, from_obj, 0, TO_ROOM);
      act("You empty $p.", FALSE, ch, from_obj, 0, TO_CHAR);

      weight_change_object(from_obj, -GET_OBJ_VAL(from_obj, 1));        /* Empty */

      GET_OBJ_VAL(from_obj, 1) = 0;
      GET_OBJ_VAL(from_obj, 2) = 0;
      GET_OBJ_VAL(from_obj, 3) = 0;
      name_from_drinkcon(from_obj);

      return;
    }
    if (!(to_obj = get_obj_in_list_vis(ch, arg2, ch->carrying))) {
      send_to_char(ch, "You don't see anything named '%s' here.\r\n", arg2);
      return;
    }
    if ((GET_OBJ_TYPE(to_obj) != ITEM_DRINKCON) &&
        (GET_OBJ_TYPE(to_obj) != ITEM_FOUNTAIN)) {
      act("You can't pour anything into $p.", FALSE, ch, to_obj, 0, TO_CHAR);
      return;
    }
  }
  if (to_obj == from_obj) {
    act("A most unproductive effort.", FALSE, ch, 0, 0, TO_CHAR);
    return;
  }
  if ((GET_OBJ_VAL(to_obj, 1) != 0) &&
      (GET_OBJ_VAL(to_obj, 2) != GET_OBJ_VAL(from_obj, 2))) {
    act("There is already another liquid in it!", FALSE, ch, 0, 0, TO_CHAR);
    return;
  }
  if (!(GET_OBJ_VAL(to_obj, 1) < GET_OBJ_VAL(to_obj, 0))) {
    act("There is no room for more.", FALSE, ch, 0, 0, TO_CHAR);
    return;
  }
  if (subcmd == SCMD_POUR) {
    snprintf(buf, sizeof(buf), "You pour the %s into the %s.",
            drinknames[GET_OBJ_VAL(from_obj, 2)], arg2);
    send_to_char(buf, ch);
  }
  if (subcmd == SCMD_FILL) {
    act("You gently fill $p from $P.", FALSE, ch, to_obj, from_obj, TO_CHAR);
    act("$n gently fills $p from $P.", TRUE, ch, to_obj, from_obj, TO_ROOM);
  }
  /* New alias */
  if (GET_OBJ_VAL(to_obj, 1) == 0)
    name_to_drinkcon(to_obj, GET_OBJ_VAL(from_obj, 2));

  /* First same type liq. */
  GET_OBJ_VAL(to_obj, 2) = GET_OBJ_VAL(from_obj, 2);

  /* Then how much to pour */
  GET_OBJ_VAL(from_obj, 1) -= (amount =
                                 (GET_OBJ_VAL(to_obj, 0) - GET_OBJ_VAL(to_obj, 1)));

  GET_OBJ_VAL(to_obj, 1) = GET_OBJ_VAL(to_obj, 0);

  if (GET_OBJ_VAL(from_obj, 1) < 0) {   /* There was too little */
    GET_OBJ_VAL(to_obj, 1) += GET_OBJ_VAL(from_obj, 1);
    amount += GET_OBJ_VAL(from_obj, 1);
    GET_OBJ_VAL(from_obj, 1) = 0;
    GET_OBJ_VAL(from_obj, 2) = 0;
    GET_OBJ_VAL(from_obj, 3) = 0;
    name_from_drinkcon(from_obj);
  }
  /* Then the poison boogie */
  GET_OBJ_VAL(to_obj, 3) =
    (GET_OBJ_VAL(to_obj, 3) || GET_OBJ_VAL(from_obj, 3));

  /* And the weight boogie */
  weight_change_object(from_obj, -(int)(amount/5));
  weight_change_object(to_obj, (int)(amount/5)); /* Add weight */

  return;
}

void wear_message(struct char_data * ch, struct obj_data * obj, int where)
{
  const char *wear_messages[][2] = {
                               {"$n activates $p and holds it.",
                                "You activate $p and hold it."},

                               {"$n wears $p on $s head.",
                                "You wear $p on your head."},

                               {"$n wears $p on $s eyes.",
                                "You wear $p on your eyes."},

                               {"$n wears $p in $s ear.",
                                "You wear $p in your ear."},

                               {"$n wears $p in $s ear.",
                                "You wear $p in your ear."},

                               {"$n wears $p over $s face.",
                                "You wear $p over your face."},

                               {"$n wears $p around $s neck.",
                                "You wear $p around your neck."},

                               {"$n wears $p around $s neck.",
                                "You wear $p around your neck."},

                               {"$n casually slings $p over $s shoulder.",
                                "You sling $p over your shoulder."},

                               {"$n wears $p about $s body.",
                                "You wear $p around your body."},

                               {"$n wears $p on $s body.",
                                "You wear $p on your body.",},

                               {"$n wears $p underneath $s clothes.",
                                "You wear $p under your clothes."},

                               {"$n wears $p on $s arms.",
                                "You wear $p on your arms."},

                               {"$n slings $p under $s left arm.",
                                "You sling $p under your left arm."},

                               {"$n slings $p under $s right arm.",
                                "You sling $p under your right arm."},

                               {"$n puts $p on around $s right wrist.",
                                "You put $p on around your right wrist."},

                               {"$n puts $p on around $s left wrist.",
                                "You put $p on around your left wrist."},

                               {"$n puts $p on $s hands.",
                                "You put $p on your hands."},

                               {"$n wields $p.",
                                "You wield $p."},

                               {"$n grabs $p.",
                                "You grab $p."},

                               {"$n straps $p around $s arm as a shield.",
                                "You start to use $p as a shield."},

                               {"$n slides $p on to $s right ring finger.",
                                "You slide $p on to your right ring finger."},

                               {"$n slides $p on to $s left ring finger.",
                                "You slide $p on to your left ring finger."},

                               {"$n slides $p onto $s right index finger.",
                                "You slide $p onto your right index finger."},

                               {"$n slides $p onto $s left index finger.",
                                "You slide $p onto your left index finger."},

                               {"$n slides $p onto $s right middle finger.",
                                "You slide $p onto your right middle finger."},

                               {"$n slides $p onto $s left middle finger.",
                                "You slide $p onto your left middle finger."},

                               {"$n slides $p onto $s right pinkie finger.",
                                "You slide $p onto your right pinkie finger."},

                               {"$n slides $p onto $s left pinkie finger.",
                                "You slide $p onto your left pinkie finger."},

                               {"$n wears $p in $s belly button.",
                                "You put $p in your belly button."},

                               {"$n wears $p around $s waist.",
                                "You wear $p around your waist."},

                               {"$n puts $p around $s thigh.",
                                "You put $p around your thigh."},

                               {"$n puts $p around $s thigh.",
                                "You put $p around your thigh."},

                               {"$n puts $p on $s legs.",
                                "You put $p on your legs."},

                               {"$n puts $p around $s left ankle.",
                                "You put $p around your left ankle."},

                               {"$n puts $p around $s right ankle.",
                                "You put $p around your right ankle."},

                               {"$n wears $p on $s feet.",
                                "You wear $p on your feet."},

                               {"$n wears $p on $s feet.",
                                "You put $p on your feet."}

                              /*
                                {"$n sticks $p in $s mouth.",
                                "You stick $p in your mouth."}*/
                             };

  /* Should we add move waer types?*/
  if (where == WEAR_WIELD || where == WEAR_HOLD)
  {
    if (GET_OBJ_TYPE(obj) == ITEM_WEAPON) {
      act(wear_messages[WEAR_WIELD][0], TRUE, ch, obj, 0, TO_ROOM);
      act(wear_messages[WEAR_WIELD][1], FALSE, ch, obj, 0, TO_CHAR);
    } else {
      act(wear_messages[WEAR_HOLD][0], TRUE, ch, obj, 0, TO_ROOM);
      act(wear_messages[WEAR_HOLD][1], FALSE, ch, obj, 0, TO_CHAR);
    }
  } else
  {
    act(wear_messages[where][0], TRUE, ch, obj, 0, TO_ROOM);
    act(wear_messages[where][1], FALSE, ch, obj, 0, TO_CHAR);
  }
}

int can_wield_both(struct char_data *ch, struct obj_data *one, struct obj_data *two)
{
  if (!one || !two)
    return TRUE;
  if (GET_OBJ_TYPE(one) != ITEM_WEAPON || GET_OBJ_TYPE(two) != ITEM_WEAPON)
    return TRUE;
  if ((IS_GUN(GET_OBJ_VAL(one, 3)) && !IS_GUN(GET_OBJ_VAL(two, 3))) ||
      (IS_GUN(GET_OBJ_VAL(two, 3)) && !IS_GUN(GET_OBJ_VAL(one, 3))))
    return FALSE;
  else if (!IS_GUN(GET_OBJ_VAL(one, 3)) && !IS_GUN(GET_OBJ_VAL(two, 3)))
    return FALSE;
  else if (IS_OBJ_STAT(one, ITEM_EXTRA_TWOHANDS) || IS_OBJ_STAT(two, ITEM_EXTRA_TWOHANDS))
    return FALSE;

  return TRUE;
}

void perform_wear(struct char_data * ch, struct obj_data * obj, int where, bool print_messages)
{
  struct obj_data *wielded = GET_EQ(ch, WEAR_WIELD);


  const char *already_wearing[] = {
                              "You're already using a light.\r\n",
                              "You're already wearing something on your head.\r\n",
                              "You're already wearing something on your eyes.\r\n",
                              "YOU SHOULD NEVER SEE THIS MESSAGE.  PLEASE REPORT.\r\n",
                              "You can't wear anything else in your ears.\r\n",
                              "There is already something covering your face.\r\n",
                              "YOU SHOULD NEVER SEE THIS MESSAGE.  PLEASE REPORT.\r\n",
                              "You can't wear anything else around your neck.\r\n",
                              "You already have something slung over your back.\r\n",
                              "You're already wearing something about your body.\r\n",
                              "You're already wearing something on your body.\r\n",
                              "You're already wearing something under your clothes.\r\n",
                              "You're already wearing something on your arms.\r\n",
                              "YOU SHOULD NEVER SEE THIS MESSAGE.  PLEASE REPORT.\r\n",
                              "You have something under both of your arms.\r\n",
                              "YOU SHOULD NEVER SEE THIS MESSAGE.  PLEASE REPORT.\r\n",
                              "You're already wearing something around both of your wrists.\r\n",
                              "You're already wearing something on your hands.\r\n",
                              "You're already wielding a weapon.\r\n",
                              "You're already holding something.\r\n",
                              "You're already using a shield.\r\n",
                              "YOU SHOULD NEVER SEE THIS MESSAGE.  PLEASE REPORT.\r\n",
                              "YOU SHOULD NEVER SEE THIS MESSAGE.  PLEASE REPORT.\r\n",
                              "YOU SHOULD NEVER SEE THIS MESSAGE.  PLEASE REPORT.\r\n",
                              "YOU SHOULD NEVER SEE THIS MESSAGE.  PLEASE REPORT.\r\n",
                              "YOU SHOULD NEVER SEE THIS MESSAGE.  PLEASE REPORT.\r\n",
                              "YOU SHOULD NEVER SEE THIS MESSAGE.  PLEASE REPORT.\r\n",
                              "YOU SHOULD NEVER SEE THIS MESSAGE.  PLEASE REPORT.\r\n",
                              "Who do you think you are? Mr. T?.\r\n",
                              "You already have something in your belly button.\r\n",
                              "You already have something around your waist.\r\n",
                              "YOU SHOULD NEVER SEE THIS MESSAGE.  PLEASE REPORT.\r\n",
                              "You are already wearing something around both or your thighs.\r\n",
                              "You're already wearing something on your legs.\r\n",
                              "YOU SHOULD NEVER SEE THIS MESSAGE.  PLEASE REPORT.\r\n",
                              "You already have something on each of your ankles.\r\n",
                              "You are already wearing something on your feet.\r\n",
                              "You're already wearing something on your feet.\r\n",
                              /*"You already have something in your mouth.\r\n" */
                            };

  /* first, make sure that the wear position is valid. */
  if (!CAN_WEAR(obj, wear_bitvectors[where]))
  {
    if (print_messages)
      act("You can't wear $p there.", FALSE, ch, obj, 0, TO_CHAR);
    return;
  }
  switch (GET_RACE(ch))
  {
  case RACE_WAKYAMBI:
  case RACE_DRYAD:
  case RACE_ELF:
  case RACE_NIGHTONE:
    if (IS_OBJ_STAT(obj, ITEM_EXTRA_NOELF)) {
      if (print_messages)
        send_to_char(ch, "%s isn't sized right for elves.\r\n", capitalize(GET_OBJ_NAME(obj)));
      return;
    }
    break;
  case RACE_DWARF:
  case RACE_MENEHUNE:
  case RACE_GNOME:
  case RACE_KOBOROKURU:
    if (IS_OBJ_STAT(obj, ITEM_EXTRA_NODWARF)) {
      if (print_messages)
        send_to_char(ch, "%s isn't sized right for dwarfs.\r\n", capitalize(GET_OBJ_NAME(obj)));
      return;
    }
    break;
  case RACE_TROLL:
  case RACE_MINOTAUR:
  case RACE_GIANT:
  case RACE_FOMORI:
  case RACE_CYCLOPS:
    if (IS_OBJ_STAT(obj, ITEM_EXTRA_NOTROLL)) {
      if (print_messages)
        send_to_char(ch, "%s isn't sized right for trolls.\r\n", capitalize(GET_OBJ_NAME(obj)));
      return;
    }
    break;
  case RACE_HUMAN:
    if (IS_OBJ_STAT(obj, ITEM_EXTRA_NOHUMAN)) {
      if (print_messages)
        send_to_char(ch, "%s isn't sized right for humans.\r\n", capitalize(GET_OBJ_NAME(obj)));
      return;
    }
    break;
  case RACE_ONI:
  case RACE_ORK:
  case RACE_OGRE:
  case RACE_HOBGOBLIN:
    if (IS_OBJ_STAT(obj, ITEM_EXTRA_NOORK)) {
      if (print_messages)
        send_to_char(ch, "%s isn't sized right for orks.\r\n", capitalize(GET_OBJ_NAME(obj)));
      return;
    }
    break;
  }
  /* for neck wrist ankles and ears, fingers moved to next case */
  if ((where == WEAR_NECK_1) || (where == WEAR_WRIST_R) || (where == WEAR_LARM) ||
      (where == WEAR_LANKLE) || (where == WEAR_EAR) || (where == WEAR_THIGH_R))
    if (GET_EQ(ch, where))
      where++;

  if (where == WEAR_FINGER_R) {
    while ((GET_EQ(ch, where)) && (where < WEAR_FINGER8))
      if (where == WEAR_FINGER_L)
        where = WEAR_FINGER3;
      else
        where++;
  }


  if ((where == WEAR_WIELD || where == WEAR_HOLD) && IS_OBJ_STAT(obj, ITEM_EXTRA_TWOHANDS) &&
      (GET_EQ(ch, WEAR_SHIELD) || GET_EQ(ch, WEAR_HOLD) || GET_EQ(ch, WEAR_WIELD)))
  {
    if (print_messages)
      act("$p requires two free hands.", FALSE, ch, obj, 0, TO_CHAR);
    return;
  }

  if (where == WEAR_WIELD)
  {
    if (!wielded) {
      if (!can_wield_both(ch, obj, GET_EQ(ch, WEAR_HOLD))) {
        if (print_messages) {
          snprintf(buf, sizeof(buf), "You'll have a hard time wielding %s along with $p.", GET_OBJ_NAME(obj));
          act(buf, FALSE, ch, GET_EQ(ch, WEAR_HOLD), 0, TO_CHAR);
        }
        return;
      }
    } else {
      /* if attempting to wield a second weapon... */
      if (GET_EQ(ch, WEAR_HOLD) || GET_EQ(ch, WEAR_SHIELD)) {
        if (print_messages)
          send_to_char("Your hands are already full!\r\n", ch);
        return;
      }
      where = WEAR_HOLD;
      if (!can_wield_both(ch, wielded, obj)) {
        if (print_messages) {
          snprintf(buf, sizeof(buf), "You'll have a hard time wielding %s along with $p.", GET_OBJ_NAME(obj));
          act(buf, FALSE, ch, wielded, 0, TO_CHAR);
        }
        return;
      }
    }
  } else if (GET_EQ(ch, where))
  {
    send_to_char(already_wearing[where], ch);
    return;
  }

  if (GET_EQ(ch, WEAR_HOLD) && where == WEAR_SHIELD)
  {
    if (print_messages)
      act("$p requires at least one free hand.", FALSE, ch, obj, 0, TO_CHAR );
    return;
  }

  if (GET_EQ(ch, WEAR_WIELD) && IS_OBJ_STAT(GET_EQ(ch, WEAR_WIELD), ITEM_EXTRA_TWOHANDS) &&
      (where == WEAR_HOLD || where == WEAR_SHIELD))
  {
    if (print_messages)
      act("$p requires two free hands.", FALSE, ch, GET_EQ(ch, WEAR_WIELD), 0, TO_CHAR);
    return;
  } else if (GET_EQ(ch, WEAR_HOLD) && IS_OBJ_STAT(GET_EQ(ch, WEAR_HOLD), ITEM_EXTRA_TWOHANDS) &&
             (where == WEAR_WIELD || where == WEAR_SHIELD))
  {
    if (print_messages)
      act("$p requires two free hands.", FALSE, ch, GET_EQ(ch, WEAR_HOLD), 0, TO_CHAR);
    return;
  }

  // Iterate through what they're wearing and check for compatibility.
  struct obj_data *worn_item = NULL;
  for (int wearloc = 0; wearloc < NUM_WEARS; wearloc++) {
    // They're allowed to wear helmets.
    if (wearloc == WEAR_HEAD)
      continue;

    // Not wearing anything here? Skip.
    if (!(worn_item = GET_EQ(ch, wearloc)))
      continue;

    // If this item can't be worn with other armors, check to make sure we meet that restriction.
    if ((IS_OBJ_STAT(obj, ITEM_EXTRA_BLOCKS_ARMOR) || IS_OBJ_STAT(obj, ITEM_EXTRA_HARDENED_ARMOR)) &&
        (GET_OBJ_TYPE(worn_item) == ITEM_WORN && (GET_WORN_IMPACT(worn_item) || GET_WORN_BALLISTIC(worn_item)))) {
      if (print_messages)
        send_to_char(ch, "You can't wear %s with %s.\r\n", GET_OBJ_NAME(obj), GET_OBJ_NAME(worn_item));
      return;
    }

    // If what they're wearing blocks other armors, and this item is armored, fail.
    if ((IS_OBJ_STAT(worn_item, ITEM_EXTRA_BLOCKS_ARMOR) || IS_OBJ_STAT(worn_item, ITEM_EXTRA_HARDENED_ARMOR)) &&
        (GET_OBJ_TYPE(obj) == ITEM_WORN && (GET_WORN_IMPACT(obj) || GET_WORN_BALLISTIC(obj)))) {
      if (print_messages)
        send_to_char(ch, "You can't wear %s with %s.\r\n", GET_OBJ_NAME(obj), GET_OBJ_NAME(worn_item));
      return;
    }
  }

  if (print_messages)
    wear_message(ch, obj, where);
  if (obj->in_obj)
    obj_from_obj(obj);
  else
    obj_from_char(obj);
  equip_char(ch, obj, where);

  if (print_messages) {
    switch (get_armor_penalty_grade(ch)) {
      case ARMOR_PENALTY_TOTAL:
        send_to_char("You are wearing so much armor that you can't move!\r\n", ch);
        break;
      case ARMOR_PENALTY_HEAVY:
        send_to_char("Your movement is severely restricted by your armor.\r\n", ch);
        break;
      case ARMOR_PENALTY_MEDIUM:
        send_to_char("Your movement is restricted by your armor.\r\n", ch);
        break;
      case ARMOR_PENALTY_LIGHT:
        send_to_char("Your movement is mildly restricted by your armor.\r\n", ch);
        break;
    }
  }
}

int find_eq_pos(struct char_data * ch, struct obj_data * obj, char *arg)
{
  int where = -1;

  static const char *keywords[] =
    {
      "!RESERVED!",
      "head",
      "eyes",
      "ear",
      "!RESERVED!",
      "face",
      "neck",
      "!RESERVED!",
      "back",
      "about",
      "body",
      "underneath",
      "arms",
      "underarm",
      "!RESERVED!",
      "wrist",
      "!RESERVED!",
      "hands",
      "!RESERVED!",
      "!RESERVED!",
      "shield",
      "finger",
      "!RESERVED!",
      "!RESERVED!",
      "!RESERVED!",
      "!RESERVED!",
      "!RESERVED!",
      "!RESERVED!",
      "!RESERVED!",
      "belly",
      "waist",
      "thigh",
      "thigh",
      "legs",
      "ankle",
      "!RESERVED!",
      "sock",
      "feet",
      // "mouth",
      "\n"
    };

  if (arg && *arg) {
    if ((where = search_block(arg, keywords, FALSE)) >= 0)
      return where;
    else
      send_to_char(ch, "'%s' isn't a part of your body, so you decide to improvise.\r\n", arg);
  }

  if (CAN_WEAR(obj, ITEM_WEAR_FINGER))
    where = WEAR_FINGER_R;
  if (CAN_WEAR(obj, ITEM_WEAR_NECK))
    where = WEAR_NECK_1;
  if (CAN_WEAR(obj, ITEM_WEAR_BODY))
    where = WEAR_BODY;
  if (CAN_WEAR(obj, ITEM_WEAR_HEAD))
    where = WEAR_HEAD;
  if (CAN_WEAR(obj, ITEM_WEAR_LEGS))
    where = WEAR_LEGS;
  if (CAN_WEAR(obj, ITEM_WEAR_FEET))
    where = WEAR_FEET;
  if (CAN_WEAR(obj, ITEM_WEAR_HANDS))
    where = WEAR_HANDS;
  if (CAN_WEAR(obj, ITEM_WEAR_ARMS))
    where = WEAR_ARMS;
  if (CAN_WEAR(obj, ITEM_WEAR_SHIELD))
    where = WEAR_SHIELD;
  if (CAN_WEAR(obj, ITEM_WEAR_ABOUT))
    where = WEAR_ABOUT;
  if (CAN_WEAR(obj, ITEM_WEAR_WAIST))
    where = WEAR_WAIST;
  if (CAN_WEAR(obj, ITEM_WEAR_WRIST))
    where = WEAR_WRIST_R;
  if (CAN_WEAR(obj, ITEM_WEAR_EYES))
    where = WEAR_EYES;
  if (CAN_WEAR(obj, ITEM_WEAR_EAR))
    where = WEAR_EAR;
  if (CAN_WEAR(obj, ITEM_WEAR_UNDER))
    where = WEAR_UNDER;
  if (CAN_WEAR(obj, ITEM_WEAR_ANKLE))
    where = WEAR_LANKLE;
  if (CAN_WEAR(obj, ITEM_WEAR_SOCK))
    where = WEAR_SOCK;
  if (CAN_WEAR(obj, ITEM_WEAR_BACK))
    where = WEAR_BACK;
  if (CAN_WEAR(obj, ITEM_WEAR_BELLY))
    where = WEAR_BELLY;
  if (CAN_WEAR(obj, ITEM_WEAR_ARM))
    where = WEAR_LARM;
  if (CAN_WEAR(obj, ITEM_WEAR_FACE))
    where = WEAR_FACE;
  if (CAN_WEAR(obj, ITEM_WEAR_THIGH))
    where = WEAR_THIGH_R;
/*
  if (CAN_WEAR(obj, ITEM_WEAR_MOUTH))
    where = WEAR_MOUTH;
*/

  return where;
}

ACMD(do_wear)
{
  char arg1[MAX_INPUT_LENGTH];
  char arg2[MAX_INPUT_LENGTH];
  struct obj_data *obj, *next_obj;
  int where = -1, dotmode, items_worn = 0;

  two_arguments(argument, arg1, arg2);

  if (AFF_FLAGGED(ch, AFF_PILOT)) {
    send_to_char("While driving? Now that would be a good trick!\r\n", ch);
    return;
  }

  if (!*arg1) {
    send_to_char("Wear what?\r\n", ch);
    return;
  }
  dotmode = find_all_dots(arg1, sizeof(arg1));

  if (*arg2 && (dotmode != FIND_INDIV)) {
    send_to_char("You can't specify the same body location for more than one item!\r\n", ch);
    return;
  }
  if (dotmode == FIND_ALL) {
    for (obj = ch->carrying; obj; obj = next_obj) {
      next_obj = obj->next_content;
      if (CAN_SEE_OBJ(ch, obj) && (where = find_eq_pos(ch, obj, 0)) >= 0) {
        items_worn++;
        perform_wear(ch, obj, where, TRUE);
      }
    }
    if (!items_worn)
      send_to_char("You don't seem to have anything wearable in your inventory.\r\n", ch);
  } else if (dotmode == FIND_ALLDOT) {
    if (!*arg1) {
      send_to_char("Wear all of what?\r\n", ch);
      return;
    }
    if (!(obj = get_obj_in_list_vis(ch, arg1, ch->carrying))) {
      send_to_char(ch, "You don't seem to have any %ss in your inventory.\r\n", arg1);
    } else
      while (obj) {
        next_obj = get_obj_in_list_vis(ch, arg1, obj->next_content);
        if ((where = find_eq_pos(ch, obj, 0)) >= 0)
          perform_wear(ch, obj, where, TRUE);
        else
          act("You can't wear $p.", FALSE, ch, obj, 0, TO_CHAR);
        obj = next_obj;
      }
  } else {
    if (!(obj = get_obj_in_list_vis(ch, arg1, ch->carrying))) {
      send_to_char(ch, "You don't seem to have %s %s in your inventory.\r\n", AN(arg1), arg1);
    } else {
      if ((where = find_eq_pos(ch, obj, arg2)) >= 0)
        perform_wear(ch, obj, where, TRUE);
      else if (!*arg2)
        act("You can't wear $p.", FALSE, ch, obj, 0, TO_CHAR);
    }
  }
}

ACMD(do_wield)
{
  struct obj_data *obj, *attach;
  rnum_t rnum;
  one_argument(argument, arg);

  if (!*arg)
    send_to_char("Wield what?\r\n", ch);
  else if (!(obj = get_obj_in_list_vis(ch, arg, ch->carrying))) {
    send_to_char(ch, "You don't seem to have %s %s in your inventory.\r\n", AN(arg), arg);
  } else {
    if (!CAN_WEAR(obj, ITEM_WEAR_WIELD))
      send_to_char(ch, "You can't wield %s.\r\n", GET_OBJ_NAME(obj));
    else if (GET_OBJ_TYPE(obj) == ITEM_WEAPON
             && !IS_GUN(GET_WEAPON_ATTACK_TYPE(obj))
             && GET_WEAPON_FOCUS_BONDED_BY(obj) == GET_IDNUM(ch)
             && GET_MAG(ch) * 2 < GET_WEAPON_FOCUS_RATING(obj))
    {
      send_to_char(ch, "%s is too powerful for you to wield!\r\n", capitalize(GET_OBJ_NAME(obj)));
      return;
    }
    else if (GET_OBJ_VAL(obj, 4) >= SKILL_MACHINE_GUNS && GET_OBJ_VAL(obj, 4) <= SKILL_ASSAULT_CANNON &&
             (GET_STR(ch) < 8 || GET_BOD(ch) < 8)) {
      bool found = FALSE;
      for (int i = 0; i < (NUM_WEARS - 1); i++)
        if (GET_EQ(ch, i) && GET_OBJ_TYPE(GET_EQ(ch, i)) == ITEM_GYRO)
          found = TRUE;
      if (GET_OBJ_VAL(obj, 9) && (rnum = real_object(GET_OBJ_VAL(obj, 9))) > -1 &&
        (attach = &obj_proto[rnum]) && GET_OBJ_TYPE(attach) == ITEM_GUN_ACCESSORY && (GET_OBJ_VAL(attach, 1) == ACCESS_BIPOD || GET_OBJ_VAL(attach, 1) == ACCESS_TRIPOD))
        found = TRUE;

      if (!found)
        send_to_char(ch, "Warning: %s is too heavy for you to wield effectively in combat unless you're ^WPRONE^n.\r\n", decapitalize_a_an(GET_OBJ_NAME(obj)));

      perform_wear(ch, obj, WEAR_WIELD, TRUE);
    } else
      perform_wear(ch, obj, WEAR_WIELD, TRUE);
  }
}

ACMD(do_grab)
{
  struct obj_data *obj;

  one_argument(argument, arg);

  if (!*arg)
    send_to_char("Hold what?\r\n", ch);
  else if (!(obj = get_obj_in_list_vis(ch, arg, ch->carrying))) {
    send_to_char(ch, "You don't seem to have %s %s in your inventory.\r\n", AN(arg), arg);
  } else {
    if (GET_OBJ_TYPE(obj) == ITEM_LIGHT)
      perform_wear(ch, obj, WEAR_LIGHT, TRUE);

    // Auto-wield if it's not holdable but is wieldable.
    else if (!CAN_WEAR(obj, wear_bitvectors[WEAR_HOLD]) && CAN_WEAR(obj, wear_bitvectors[WEAR_WIELD]))
      perform_wear(ch, obj, WEAR_WIELD, TRUE);

    // Hold.
    else
      perform_wear(ch, obj, WEAR_HOLD, TRUE);
  }
}

void perform_remove(struct char_data * ch, int pos)
{
  struct obj_data *obj;

  if (!(obj = ch->equipment[pos]))
  {
    log("Error in perform_remove: bad pos passed.");
    return;
  }
  if (IS_CARRYING_N(ch) >= CAN_CARRY_N(ch)) {
    act("$p: you can't carry that many items!", FALSE, ch, obj, 0, TO_CHAR);
    return;
  }

  int previous_armor_penalty = get_armor_penalty_grade(ch);

  obj_to_char(unequip_char(ch, pos, TRUE), ch);
  act("You stop using $p.", FALSE, ch, obj, 0, TO_CHAR);
  act("$n stops using $p.", TRUE, ch, obj, 0, TO_ROOM);

  if (previous_armor_penalty && !get_armor_penalty_grade(ch))
    send_to_char("You can move freely again.\r\n", ch);

  // Unready the holster.
  if (GET_OBJ_TYPE(obj) == ITEM_HOLSTER)
    GET_HOLSTER_READY_STATUS(obj) = 0;
  /* add damage back from stim patches */
  /* it doesn't do anything to keep track, */
  /* so I'm just makeing it a mod mental damage to it */
  else if ( GET_OBJ_TYPE(obj) == ITEM_PATCH && GET_OBJ_VAL(obj, 0) == 1 ) {
    GET_MENTAL(ch) = GET_OBJ_VAL(obj,5);
    GET_OBJ_VAL(obj,5) = 0;
  }

  return;
}

ACMD(do_remove)
{
  struct obj_data *obj;
  int i, dotmode, found;

  one_argument(argument, arg);

  if (AFF_FLAGGED(ch, AFF_PILOT)) {
    send_to_char("While driving? Now that would be a good trick!\r\n", ch);
    return;
  }

  if (!*arg) {
    send_to_char("Remove what?\r\n", ch);
    return;
  }

  dotmode = find_all_dots(arg, sizeof(arg));

  if (dotmode == FIND_ALL) {
    found = 0;
    for (i = 0; i < NUM_WEARS; i++)
      if (GET_EQ(ch, i)) {
        perform_remove(ch, i);
        found = 1;
      }
    if (!found)
      send_to_char("You're not using anything.\r\n", ch);
  } else if (dotmode == FIND_ALLDOT) {
    if (!*arg)
      send_to_char("Remove all of what?\r\n", ch);
    else {
      found = 0;
      for (i = 0; i < NUM_WEARS; i++)
        if (GET_EQ(ch, i) && CAN_SEE_OBJ(ch, GET_EQ(ch, i)) &&
            isname(arg, GET_EQ(ch, i)->text.keywords)) {
          if (GET_OBJ_TYPE(GET_EQ(ch, i)) == ITEM_GYRO) {
            if (GET_EQ(ch, WEAR_WIELD))
              perform_remove(ch, WEAR_WIELD);
            if (GET_EQ(ch, WEAR_HOLD))
              perform_remove(ch, WEAR_HOLD);
          }
          perform_remove(ch, i);
          found = 1;
        }
      if (!found) {
        send_to_char(ch, "You don't seem to be using any %ss.\r\n", arg);
      }
    }
  } else {
    if (!(obj = get_object_in_equip_vis(ch, arg, ch->equipment, &i))) {
      send_to_char(ch, "You don't seem to be using %s %s.\r\n", AN(arg), arg);
    } else {
      if (GET_OBJ_TYPE(obj) == ITEM_GYRO) {
        if (GET_EQ(ch, WEAR_WIELD))
          perform_remove(ch, WEAR_WIELD);
        if (GET_EQ(ch, WEAR_HOLD))
          perform_remove(ch, WEAR_HOLD);
      }
      perform_remove(ch, i);
    }
  }
}

ACMD(do_activate)
{
  struct obj_data *obj;
  int i;

  if (!*argument) {
    send_to_char("What do you want to activate?\r\n", ch);
    return;
  }

  if (GET_TRADITION(ch) == TRAD_ADEPT && !str_str(argument, "Pain editor") && !str_str(argument, "voice modulator")) {
    char name[120], tokens[MAX_STRING_LENGTH], *s;
    extern int ability_cost(int abil, int level);
    int x;
    strncpy(tokens, argument, sizeof(tokens) - 1);
    if ((strtok(tokens, "\"") && (s = strtok(NULL, "\"")))
        || (strtok(tokens, "'") && (s = strtok(NULL, "'")))) {
      strncpy(name, s, sizeof(name) - 1);
      if ((s = strtok(NULL, "\0"))) {
        skip_spaces(&s);
        strncpy(buf1, s, sizeof(buf1) - 1);
      } else
        *buf1 = '\0';
      one_argument(argument, buf);
      x = atoi(buf);
    } else {
      half_chop(argument, buf, buf1);
      if (!(x = atoi(buf))) {
        strncpy(name, buf, sizeof(name) - 1);
      } else {
        half_chop(buf1, buf2, buf1);
        strncpy(name, buf2, sizeof(name) - 1);
      }
    }

    // Find powers they have skill in first.
    for (i = 0; i < ADEPT_NUMPOWER; i++)
      if (GET_POWER_TOTAL(ch, i) && is_abbrev(name, adept_powers[i]))
        break;
    // Find powers they don't have skill in.
    if (i >= ADEPT_NUMPOWER)
      for (i = 0; i < ADEPT_NUMPOWER; i++)
        if (is_abbrev(name, adept_powers[i]))
          if (!GET_POWER_TOTAL(ch, i)) {
            send_to_char(ch, "You haven't learned the %s power yet.\r\n", adept_powers[i]);
            return;
          }

    if (i < ADEPT_NUMPOWER) {
      if (x == 0)
        x = GET_POWER_TOTAL(ch, i);
      else
        x = MIN(x, GET_POWER_TOTAL(ch, i));
      int total = 0;
      for (int q = x > GET_POWER_ACT(ch, i) ? x : GET_POWER_ACT(ch, i);
                   x > GET_POWER_ACT(ch, i) ? GET_POWER_ACT(ch, i) < q : x < q; q--)
        total += ability_cost(i, q);
      if (x < GET_POWER_ACT(ch, i))
        total *= -1;

      int delta = ((int)(GET_REAL_MAG(ch) / 100) * 100) - GET_POWER_POINTS(ch);
      if (total > delta)
        send_to_char(ch, "That costs %d points to activate, but you only have %d free.\r\n", (total / 100), (delta / 100));
      else if (GET_POWER_ACT(ch, i) == x) {
        send_to_char(ch, "%s is already active at rank %d.\r\n", CAP(adept_powers[i]), x);
        return;
      } else {
        GET_POWER_ACT(ch, i) = x;
        GET_POWER_POINTS(ch) += total;
        if (i == ADEPT_BOOST_BOD || i == ADEPT_BOOST_QUI || i == ADEPT_BOOST_STR) {
          send_to_char(ch, "You activate %s. You'll need to use the ^WBOOST^n command to engage it.\r\n", adept_powers[i]);
        } else if (i == ADEPT_NERVE_STRIKE) {
          send_to_char(ch, "You activate %s. You'll need to use the ^WNERVESTRIKE^n command to engage it.\r\n", adept_powers[i]);
        } else if (i == ADEPT_LIVINGFOCUS) {
          send_to_char(ch, "You activate %s. You'll need to use the ^WFOCUS^n command to engage it.\r\n", adept_powers[i]);
        } else {
          send_to_char(ch, "You activate %s.\r\n", adept_powers[i]);
        }
        affect_total(ch);
      }
      return;
    }
  }

  any_one_arg(argument, arg);

  if (is_abbrev(arg, "pain editor")) {
    for (obj = ch->bioware; obj; obj = obj->next_content)
      if (GET_BIOWARE_TYPE(obj) == BIO_PAINEDITOR) {
        if (GET_BIOWARE_IS_ACTIVATED(obj))
          send_to_char("You have already activated your Pain Editor.\r\n", ch);
        else {
          GET_BIOWARE_IS_ACTIVATED(obj) = 1;
          send_to_char("You lose all tactile perception.\r\n", ch);
        }
        return;
      }
  }
  if (is_abbrev(arg, "voice modulator")) {
    for (obj = ch->cyberware; obj; obj = obj->next_content)
      if (GET_CYBERWARE_TYPE(obj) == CYB_VOICEMOD) {
        if (GET_CYBERWARE_FLAGS(obj))
          send_to_char("You have already activated your Voice Modulator.\r\n", ch);
        else {
          GET_CYBERWARE_FLAGS(obj) = 1;
          send_to_char("You begin to speak like Stephen Hawking.\r\n", ch);
        }
        return;
      }
  }

  if (!(obj = get_obj_in_list_vis(ch, arg, ch->carrying))) {
    if (!(obj = get_object_in_equip_vis(ch, arg, ch->equipment, &i))) {
      if (!(obj = get_obj_in_list_vis(ch, arg, ch->cyberware))) {
        send_to_char(ch, "You don't have %s %s.\r\n", AN(arg), arg);
        return;
      }
    }
  }

  if (GET_OBJ_TYPE(obj) == ITEM_CYBERWARE) {
    switch (GET_CYBERWARE_TYPE(obj)) {
      case CYB_ARMS:
        if (IS_SET(GET_CYBERWARE_FLAGS(obj), ARMS_MOD_GYROMOUNT)) {
          if (GET_CYBERWARE_IS_DISABLED(obj)) {
            send_to_char(ch, "You activate the gyromount on %s.\r\n", decapitalize_a_an(GET_OBJ_NAME(obj)));
          } else {
            send_to_char(ch, "%s's gyromount was already activated.\r\n", CAP(GET_OBJ_NAME(obj)));
          }
          GET_CYBERWARE_IS_DISABLED(obj) = FALSE;
          return;
        }
        send_to_char(ch, "%s doesn't have a gyromount to activate.\r\n", CAP(GET_OBJ_NAME(obj)));
        return;
    }
    send_to_char(ch, "You can't activate %s.\r\n", decapitalize_a_an(GET_OBJ_NAME(obj)));
    return;
  }

  if (GET_OBJ_TYPE(obj) == ITEM_FOCUS && GET_IDNUM(ch) == GET_OBJ_VAL(obj, 2)) {
    if (obj->worn_on == NOWHERE) {
      send_to_char("You have to be wearing or holding a focus to activate it.\r\n", ch);
      return;
    }
    if (GET_OBJ_VAL(obj, 9) > 0) {
      send_to_char(ch, "You haven't finished bonding %s yet.\r\n", GET_OBJ_NAME(obj));
      return;
    }
    if (GET_OBJ_VAL(obj, 0) == FOCI_SUSTAINED) {
      send_to_char("This focus is automatically activated when you cast the bonded spell on it.\r\n", ch);
      return;
    }
    if (GET_FOCI(ch) >= GET_INT(ch)) {
      send_to_char("You have too many foci activated already.\r\n", ch);
      return;
    }
    if (GET_OBJ_VAL(obj, 4)) {
      send_to_char("This focus is already activated.\r\n", ch);
      return;
    }
    if (GET_OBJ_VAL(obj, 0) == 4)
      for (int x = 0; x < NUM_WEARS; x++)
        if (GET_EQ(ch, x) && GET_OBJ_TYPE(GET_EQ(ch, x)) == ITEM_FOCUS && GET_OBJ_VAL(GET_EQ(ch, x), 0) == 4
            && GET_OBJ_VAL(GET_EQ(ch, x), 4)) {
          send_to_char("You can only activate one power focus at a time.\r\n", ch);
          return;
        }
    send_to_char(ch, "You activate %s.\r\n", GET_OBJ_NAME(obj));
    GET_OBJ_VAL(obj, 4) = 1;
    GET_FOCI(ch)++;
    affect_total(ch);
    return;
  } else if (GET_OBJ_TYPE(obj) == ITEM_WEAPON && WEAPON_IS_FOCUS(obj)) {
    send_to_char(ch, "There's no need to activate or deactivate %s. Just wield it when you want to use it.\r\n", GET_OBJ_NAME(obj));
    return;
  } else if (GET_OBJ_TYPE(obj) != ITEM_MONEY || !GET_OBJ_VAL(obj, 1)) {
    send_to_char(ch, "You can't activate %s.\r\n", GET_OBJ_NAME(obj));
    return;
  } else if (GET_OBJ_VAL(obj, 4) != 0) {
    send_to_char(ch, "But %s has already been activated!\r\n", GET_OBJ_NAME(obj));
    return;
  }

  if (!IS_NPC(ch)) {
    GET_OBJ_VAL(obj, 3) = 1;
    GET_OBJ_VAL(obj, 4) = GET_IDNUM(ch);
  } else {
    GET_OBJ_VAL(obj, 3) = 0;
    GET_OBJ_VAL(obj, 4) = ch->nr;
  }

  switch (GET_OBJ_VAL(obj, 2)) {
  case 1:
    GET_OBJ_VAL(obj, 5) = (number(1, 9) * 100000) + (number(0, 9) * 10000) +
                          (number(0, 9) * 1000) + (number(0, 9) * 100) +
                          (number(0, 9) * 10) + number(0, 9);
    send_to_char(ch, "You key %d as the passcode and the display flashes green.\r\n", GET_OBJ_VAL(obj,5));
    break;
  case 2:
    send_to_char("You press your thumb against the pad and the display flashes green.\r\n", ch);
    break;
  case 3:
    act("$p scans your retina and the display flashes green.", FALSE, ch, obj, 0, TO_CHAR);
    break;
  }
}

ACMD(do_type)
{
  struct obj_data *obj;
  int i;

  any_one_arg(any_one_arg(argument, arg), buf);

  if (!*arg || !*buf) {
    send_to_char("Type what into what?\r\n", ch);
    return;
  }

  if (!(obj = get_obj_in_list_vis(ch, arg, ch->carrying)))
    if (!(obj = get_object_in_equip_vis(ch, arg, ch->equipment, &i))) {
      send_to_char(ch, "You don't have %s %s.\r\n", AN(arg), arg);
      return;
    }

  if (GET_OBJ_TYPE(obj) != ITEM_MONEY || !GET_OBJ_VAL(obj, 1) || GET_OBJ_VAL(obj, 2) != 1) {
    send_to_char(ch, "You can't type a code into %s.\r\n", GET_OBJ_NAME(obj));
    return;
  }

  i = atoi(buf);

  if (i < 100000 || i > 999999)
    send_to_char("Code must be 6-digits!\r\n", ch);
  else if (i != GET_OBJ_VAL(obj, 5)) {
    send_to_char("The display flashes red and beeps annoyingly.\r\n", ch);
    WAIT_STATE(ch, 2 RL_SEC);
  }
  else {
    send_to_char("The display flashes green.\r\n", ch);
    GET_OBJ_VAL(obj, 3) = 0;
    GET_OBJ_VAL(obj, 4) = 0;
    GET_OBJ_VAL(obj, 5) = 0;
  }
}

ACMD(do_crack)
{
  int skill = SKILL_ELECTRONICS, mod = 0, rating;
  struct obj_data *obj, *shop = NULL;
  if (GET_POS(ch) == POS_FIGHTING) {
    send_to_char("Good luck...\r\n", ch);
    return;
  }
  skill = get_skill(ch, skill, mod);

  any_one_arg(argument, buf);
  if (IS_WORKING(ch)) {
    send_to_char(TOOBUSY, ch);
    return;
  }
  if (!*buf) {
    send_to_char("What do you want to crack?\r\n", ch);
    return;
  }

  if (!(obj = get_obj_in_list_vis(ch, buf, ch->carrying))) {
    send_to_char(ch, "You don't have %s %s.\r\n", AN(buf), buf);
    return;
  }

  if (GET_OBJ_TYPE(obj) != ITEM_MONEY
      || !GET_OBJ_VAL(obj, 1)
      || GET_OBJ_VAL(obj, 2) > 3
      || !GET_OBJ_VAL(obj, 2)) {
    send_to_char(ch, "%s isn't someone else's activated credstick, not much cracking to be done there.\r\n", capitalize(GET_OBJ_NAME(obj)));
    return;
  } else if (!GET_OBJ_VAL(obj, 4)) {
    act("But $p's not even activated!", FALSE, ch, obj, 0, TO_CHAR);
    return;
  }

  rating = GET_OBJ_VAL(obj, 2) * 3 + 6 + mod + (GET_OBJ_ATTEMPT(obj) * 2);
  if ((shop = find_workshop(ch, TYPE_ELECTRONIC)))
    rating -= 2;

  if (!shop && !has_kit(ch, TYPE_ELECTRONIC)) {
    send_to_char("You need at least an electronics kit to crack a credstick.\r\n", ch);
    return;
  }

  if (1) {
    char rbuf[MAX_STRING_LENGTH];
    snprintf(rbuf, sizeof(rbuf), "Crack: Skill %d, Rating %d, Modify Target %d.\n",
            skill, rating, modify_target(ch));
    act(rbuf, FALSE, ch, NULL, NULL, TO_ROLLS);
  }

  // If you've attempted this so much that your TN is 100+, just stop. This prevents DB failure on attempts > 255.
  if (rating >= 100) {
    act("$p's locked itself out from too many attempts! Try again later.", FALSE, ch, obj, 0, TO_CHAR);
    return;
  }

  if (success_test(skill, rating + modify_target(ch)) < 1) {
    act("$p sounds a series of beeps, and flashes red.", FALSE, ch, obj, 0, TO_CHAR);
    GET_OBJ_ATTEMPT(obj)++;
  } else {
    act("$p hums silently, and its display flashes green.", FALSE, ch, obj, 0, TO_CHAR);
    GET_OBJ_TIMER(obj) = GET_OBJ_VAL(obj, 3) = GET_OBJ_VAL(obj, 4) = GET_OBJ_VAL(obj, 5) = 0;
  }
}

// Draw a weapon from the provided holster. Returns 1 if drawn, 0 if not.
int draw_from_readied_holster(struct char_data *ch, struct obj_data *holster) {
  struct obj_data *contents = holster->contains;

  if (!contents) {
    // Readied holster was empty: un-ready the holster, but continue looking for a valid ready holster.
    GET_HOLSTER_READY_STATUS(holster) = 0;
    return 0;
  }

  // Check to see if it can be wielded.
  if (!CAN_WEAR(contents, ITEM_WEAR_WIELD))
    return 0;

  // Did we fill up our hands, or do we only have one free hand for a two-handed weapon? Skip.
  if ((GET_EQ(ch, WEAR_WIELD) && GET_EQ(ch, WEAR_HOLD)) || ((GET_EQ(ch, WEAR_WIELD) || GET_EQ(ch, WEAR_HOLD)) && IS_OBJ_STAT(contents, ITEM_EXTRA_TWOHANDS)))
    return 0;

  // TODO: What does this check mean? (ed: probably intended to prevent machine guns and assault cannons from being drawn. Nonfunctional.)
  if (GET_OBJ_VAL(holster, 4) >= SKILL_MACHINE_GUNS && GET_OBJ_VAL(holster, 4) <= SKILL_ASSAULT_CANNON)
    return 0;

  // Refuse to let someone draw a weapon focus that is stronger than twice their magic. At least, I think that's what this does?
  if (GET_OBJ_TYPE(contents) == ITEM_WEAPON
           && !IS_GUN(GET_WEAPON_ATTACK_TYPE(contents))
           && GET_WEAPON_FOCUS_BONDED_BY(contents) == GET_IDNUM(ch)
           && GET_MAG(ch) * 2 < GET_WEAPON_FOCUS_RATING(contents))
  {
    return 0;
  }

  // Apply racial limitations.
  switch (GET_RACE(ch)) {
    case RACE_WAKYAMBI:
    case RACE_DRYAD:
    case RACE_ELF:
    case RACE_NIGHTONE:
      if (IS_OBJ_STAT(contents, ITEM_EXTRA_NOELF)) {
        return 0;
      }
      break;
    case RACE_DWARF:
    case RACE_MENEHUNE:
    case RACE_GNOME:
    case RACE_KOBOROKURU:
      if (IS_OBJ_STAT(contents, ITEM_EXTRA_NODWARF)) {
        return 0;
      }
      break;
    case RACE_TROLL:
    case RACE_MINOTAUR:
    case RACE_GIANT:
    case RACE_FOMORI:
    case RACE_CYCLOPS:
      if (IS_OBJ_STAT(contents, ITEM_EXTRA_NOTROLL)) {
        return 0;
      }
      break;
    case RACE_HUMAN:
      if (IS_OBJ_STAT(contents, ITEM_EXTRA_NOHUMAN)) {
        return 0;
      }
      break;
    case RACE_ONI:
    case RACE_ORK:
    case RACE_OGRE:
    case RACE_HOBGOBLIN:
      if (IS_OBJ_STAT(contents, ITEM_EXTRA_NOORK)) {
        return 0;
      }
      break;
  }

  // Staff-only limitations.
  if (IS_OBJ_STAT(contents, ITEM_EXTRA_STAFF_ONLY) && !access_level(ch, LVL_BUILDER)) {
    return 0;
  }

  int where = 0;
  if (!GET_EQ(ch, WEAR_WIELD) && can_wield_both(ch, GET_EQ(ch, WEAR_HOLD), contents))
    where = WEAR_WIELD;
  else if (!GET_EQ(ch, WEAR_HOLD) && can_wield_both(ch, GET_EQ(ch, WEAR_WIELD), contents))
    where = WEAR_HOLD;
  if (where) {
    obj_from_obj(contents);
    equip_char(ch, contents, where);
    act("You draw $p from $P.", FALSE, ch, contents, holster, TO_CHAR);
    act("$n draws $p from $P.", TRUE, ch, contents, holster, TO_ROOM);

    if (GET_OBJ_SPEC(contents) == weapon_dominator) {
      GET_OBJ_TYPE(contents) = ITEM_OTHER;
      dominator_mode_switch(ch, contents, DOMINATOR_MODE_PARALYZER);
    }

    // We wielded 1 weapon.
    return 1;
  }

  // We wielded 0 weapons.
  return 0;
}

int draw_weapon(struct char_data *ch)
{
  struct obj_data *potential_holster, *obj;
  int i = 0;

  //Look in fingertip first to get it out of the way.
  //At some point we need to write a mechanism to select what is drawn automatically -- Nodens
  for (potential_holster = ch->cyberware; potential_holster; potential_holster = potential_holster->next_content)
    if (GET_CYBERWARE_TYPE(potential_holster) == CYB_FINGERTIP && GET_HOLSTER_READY_STATUS(potential_holster))
      i += draw_from_readied_holster(ch, potential_holster);

  // Go through all the wearslots, provided that the character is not already wielding & holding things.
  for (int x = 0; x < NUM_WEARS && (!GET_EQ(ch, WEAR_WIELD) || !GET_EQ(ch, WEAR_HOLD)); x++) {
    if ((potential_holster = GET_EQ(ch, x))) {
      if (GET_OBJ_TYPE(potential_holster) == ITEM_HOLSTER && GET_HOLSTER_READY_STATUS(potential_holster)) {
        i += draw_from_readied_holster(ch, potential_holster);
      }

      else if (GET_OBJ_TYPE(potential_holster) == ITEM_WORN) {
        for (obj = potential_holster->contains; obj; obj = obj->next_content) {
          if (GET_OBJ_TYPE(obj) == ITEM_HOLSTER && GET_HOLSTER_READY_STATUS(obj)) {
            i += draw_from_readied_holster(ch, obj);
          }
        }
      }
    }
  }

  affect_total(ch);

  return i;
}

bool holster_can_fit(struct obj_data *holster, struct obj_data *weapon) {
  bool small_weapon = GET_WEAPON_SKILL(weapon) == SKILL_PISTOLS || GET_WEAPON_SKILL(weapon) == SKILL_SMG;
  switch (GET_HOLSTER_TYPE(holster)) {
    case HOLSTER_TYPE_SMALL_GUNS:
      // Handle standard small guns.
      if (IS_GUN(GET_WEAPON_ATTACK_TYPE(weapon)) && small_weapon)
        return TRUE;

      // Check for ranged tasers.
      return (GET_WEAPON_ATTACK_TYPE(weapon) == WEAP_TASER && GET_WEAPON_SKILL(weapon) == SKILL_TASERS);
    case HOLSTER_TYPE_MELEE_WEAPONS:
      // Handle standard melee weapons.
      if (!IS_GUN(GET_WEAPON_ATTACK_TYPE(weapon)))
        return TRUE;

      // Check for melee tasers.
      return (GET_WEAPON_ATTACK_TYPE(weapon) == WEAP_TASER && GET_WEAPON_SKILL(weapon) != SKILL_TASERS);
    case HOLSTER_TYPE_LARGE_GUNS:
      return IS_GUN(GET_WEAPON_ATTACK_TYPE(weapon)) && !small_weapon;
  }

  return FALSE;
}

struct obj_data *find_holster_that_fits_weapon(struct char_data *ch, struct obj_data *weapon) {
  //If the weapon is a monowhip see if we have a fingertip compartment and it's not currently full
  if (obj_index[GET_OBJ_RNUM(weapon)].wfunc == monowhip) {
    struct obj_data *cont;
    for (cont = ch->cyberware; cont; cont = cont->next_content)
      if (GET_CYBERWARE_TYPE(cont) == CYB_FINGERTIP && !cont->contains)
        return cont;
  }
  // Look at their worn items. We exclude inventory here.
  for (int x = 0; x < NUM_WEARS; x++) {
    // Is it a holster?
    if (GET_EQ(ch, x)
        && GET_OBJ_TYPE(GET_EQ(ch, x)) == ITEM_HOLSTER
        && !GET_EQ(ch, x)->contains
        && holster_can_fit(GET_EQ(ch, x), weapon))
    {
      return GET_EQ(ch, x);
    }

    // Does it contain a holster?
    if (GET_EQ(ch, x)
        && GET_OBJ_TYPE(GET_EQ(ch, x)) == ITEM_WORN
        && GET_EQ(ch, x)->contains)
    {
      for (struct obj_data *temp = GET_EQ(ch, x)->contains; temp; temp = temp->next_content) {
        if (GET_OBJ_TYPE(temp) == ITEM_HOLSTER
            && !temp->contains
            && holster_can_fit(temp, weapon))
        {
          return temp;
        }
      }
    }
  }

  return NULL;
}

ACMD(do_holster)
{ // Holster <gun> <holster>
  struct obj_data *obj = NULL, *cont = NULL;
  struct char_data *tmp_char;
  int dontfit = 0;

  if (IS_ASTRAL(ch)) {
    send_to_char("Astral projections can't touch things.\r\n", ch);
    return;
  }

  two_arguments(argument, buf, buf1);

  if (!argument || !*argument) {
    // Holster your wielded weapon.
    if ((obj = GET_EQ(ch, WEAR_WIELD)) || (obj = GET_EQ(ch, WEAR_HOLD))) {
      if (GET_OBJ_TYPE(obj) != ITEM_WEAPON) {
        send_to_char(ch, "%s isn't a weapon.\r\n", capitalize(GET_OBJ_NAME(obj)));
        return;
      }
    } else {
      send_to_char("You're not wielding anything.\r\n", ch);
      return;
    }

    // Find a generic holster.
    cont = find_holster_that_fits_weapon(ch, obj);
  }

  // Something was specified.
  else {
    // Find weapon.
    if (!generic_find(buf, FIND_OBJ_EQUIP | FIND_OBJ_INV, ch, &tmp_char, &obj)) {
      send_to_char(ch, "You're not carrying a '%s'.\r\n", buf);
      return;
    }

    // Find holster.
    if (!*buf1 || !generic_find(buf1, FIND_OBJ_EQUIP | FIND_OBJ_INV, ch, &tmp_char, &cont)) {
      cont = find_holster_that_fits_weapon(ch, obj);
    }
  }




  if (!cont) {
    send_to_char(ch, "You don't have any empty %s that will fit %s.\r\n", !IS_GUN(GET_OBJ_VAL(obj, 3)) ? "sheaths" : "holsters", decapitalize_a_an(GET_OBJ_NAME(obj)));
    return;
  }
  if (cont == obj) {
    send_to_char(ch, "You can't put %s inside itself.\r\n", decapitalize_a_an(GET_OBJ_NAME(obj)));
    return;
  }
  if (GET_OBJ_TYPE(obj) != ITEM_WEAPON) {
    send_to_char(ch, "%s is not a holsterable weapon.\r\n", capitalize(GET_OBJ_NAME(obj)));
    return;
  }
  if (GET_OBJ_TYPE(cont) != ITEM_HOLSTER && (GET_OBJ_TYPE(cont) != ITEM_CYBERWARE || GET_CYBERWARE_TYPE(cont) != CYB_FINGERTIP)) {
    send_to_char(ch, "%s is not a holster.\r\n", capitalize(GET_OBJ_NAME(cont)));
    return;
  }
  if (cont->contains) {
    send_to_char(ch, "There's already something in %s.\r\n", decapitalize_a_an(GET_OBJ_NAME(cont)));
    return;
  }

  const char *madefor = "<error, report to staff>";

  if (GET_OBJ_TYPE(cont) == ITEM_HOLSTER && !holster_can_fit(cont, obj)) {
    dontfit++;

    switch (GET_HOLSTER_TYPE(cont)) {
      case HOLSTER_TYPE_SMALL_GUNS:
        madefor = "pistols and SMGs";
        break;
      case HOLSTER_TYPE_MELEE_WEAPONS:
        madefor = "melee weapons";
        break;
      case HOLSTER_TYPE_LARGE_GUNS:
        madefor = "rifles and other longarms";
        break;
    }
    if (dontfit) {
      send_to_char(ch, "%s is made for %s, so %s won't fit in it.\r\n", capitalize(GET_OBJ_NAME(cont)), madefor, decapitalize_a_an(GET_OBJ_NAME(obj)));
      return;
    }
  }

  if (GET_OBJ_SPEC(obj) == weapon_dominator) {
    dominator_mode_switch(ch, obj, DOMINATOR_MODE_PARALYZER);
  }

  if (obj->worn_by)
    obj = unequip_char(ch, obj->worn_on, TRUE);
  else
    obj_from_char(obj);
  obj_to_obj(obj, cont);
  send_to_char(ch, "You slip %s into %s and ready it for a quick draw.\r\n", GET_OBJ_NAME(obj), decapitalize_a_an(GET_OBJ_NAME(cont)));
  act("$n slips $p into $P.", FALSE, ch, obj, cont, TO_ROOM);
  GET_HOLSTER_READY_STATUS(cont) = 1;
  return;
}

ACMD(do_ready)
{
  struct obj_data *obj, *finger;
  struct char_data *tmp_char;
  int num = 0;

  one_argument(argument, buf);

  if (!*buf) {
    send_to_char(ch, "You have to ready something.\r\n");
    return;
  }

  //If we have a fingertip compartment that matches the argument, set our object to that.
  if ((finger = get_obj_in_list_vis(ch, buf, ch->cyberware)) && GET_CYBERWARE_TYPE(finger) == CYB_FINGERTIP)
    obj = finger;
  else {
    if (!(generic_find(buf, FIND_OBJ_EQUIP, ch, &tmp_char, &obj))) {
      send_to_char(ch, "You don't seem to be using %s %s.\r\n", AN(argument), argument);
      return;
    }
  }
  if (GET_OBJ_TYPE(obj) != ITEM_HOLSTER && (GET_OBJ_TYPE(obj) != ITEM_CYBERWARE || GET_CYBERWARE_TYPE(obj) != CYB_FINGERTIP)) {
    send_to_char(ch, "%s is not a weapons holster.\r\n", capitalize(GET_OBJ_NAME(obj)));
    return;
  }
  if (!obj->contains) {
    send_to_char(ch, "There is nothing in %s.\r\n", GET_OBJ_NAME(obj));
    return;
  }
  if (GET_HOLSTER_READY_STATUS(obj) > 0) {
    act("You unready $p.", FALSE, ch, obj, NULL, TO_CHAR);
    GET_HOLSTER_READY_STATUS(obj) = 0;
    return;
  } else {
    //Check if we have any fingertip compartments that are readied
    for (struct obj_data *cyber = ch->cyberware; cyber; cyber = cyber->next_content)
      if (GET_CYBERWARE_TYPE(cyber) == CYB_FINGERTIP && GET_HOLSTER_READY_STATUS(cyber))
        num++;

    for (int i = 0; i < NUM_WEARS; i++)
      if (GET_EQ(ch, i)) {
        if (GET_OBJ_TYPE(GET_EQ(ch, i)) == ITEM_HOLSTER && GET_HOLSTER_READY_STATUS(GET_EQ(ch, i)) > 0)
          num++;
        else if (GET_OBJ_TYPE(GET_EQ(ch, i)) == ITEM_WORN && GET_EQ(ch, i)->contains)
          for (struct obj_data *cont = GET_EQ(ch, i)->contains; cont; cont = cont->next_content)
            if (GET_OBJ_TYPE(cont) == ITEM_HOLSTER && GET_HOLSTER_READY_STATUS(cont) > 0)
              num++;
      }
    if (num >= 2) {
      send_to_char(ch , "You already have 2 holsters readied.\r\n");
      return;
    }
    act("You ready $p.", FALSE, ch, obj, NULL, TO_CHAR);
    GET_HOLSTER_READY_STATUS(obj) = 1;
    return;
  }
}

ACMD(do_draw)
{
  if (GET_EQ(ch, WEAR_WIELD) && GET_EQ(ch, WEAR_HOLD))
    send_to_char(ch, "Your hands are full.\r\n");
  else {
    int i = draw_weapon(ch);
    if (i == 0)
      send_to_char(ch, "You have nothing to draw, or you're trying to draw a two-handed weapon with something in your hands. Make sure you're wearing a sheath or holster with a weapon in it, and that you've used the ^WREADY^n command on the sheath or holster.\r\n");
  }
}

ACMD(do_break)
{
  struct obj_data *obj = ch->cyberware, *contents = NULL;
  for (; obj; obj = obj->next_content)
    if (GET_OBJ_VAL(obj, 0) == CYB_TOOTHCOMPARTMENT && GET_OBJ_VAL(obj, 3))
      break;

  if (!obj)
    send_to_char("You don't have a breakable tooth compartment.\r\n", ch);
  else if (!(contents = obj->contains))
    send_to_char("Your tooth compartment is empty, so breaking it now would be a waste.\r\n", ch);
  else {
    extern void do_drug_take(struct char_data *ch, struct obj_data *obj);
    send_to_char("You bite down hard on the tooth compartment, breaking it open.\r\n", ch);
    obj_from_cyberware(obj);
    GET_ESSHOLE(ch) += GET_OBJ_VAL(obj, 4);
    do_drug_take(ch, contents);
    extract_obj(obj);
  }
}

ACMD(do_keep) {
  if (!argument || !*argument) {
    send_to_char("What do you want to flag as undroppable?\r\n", ch);
    return;
  }

  struct obj_data  *obj      = NULL;
  struct char_data *tmp_char = NULL;

  generic_find(argument, FIND_OBJ_EQUIP | FIND_OBJ_INV, ch, &tmp_char, &obj);

  if (!obj) {
    send_to_char(ch, "You're not carrying or wearing anything named '%s'.\r\n", argument);
    return;
  }

  if (IS_OBJ_STAT(obj, ITEM_EXTRA_KEPT)) {
    send_to_char(ch, "You un-keep %s.\r\n", decapitalize_a_an(GET_OBJ_NAME(obj)));
    GET_OBJ_EXTRA(obj).RemoveBit(ITEM_EXTRA_KEPT);
  } else {
    send_to_char(ch, "You set %s as kept. You will be unable to drop, junk, or give it away until you use this command on it again.\r\n", decapitalize_a_an(GET_OBJ_NAME(obj)));
    GET_OBJ_EXTRA(obj).SetBit(ITEM_EXTRA_KEPT);
  }

  playerDB.SaveChar(ch);
}
