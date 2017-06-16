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

#include "structs.h"
#include "utils.h"
#include "comm.h"
#include "interpreter.h"
#include "handler.h"
#include "db.h"
#include "awake.h"
#include "constants.h"

/* extern variables */
extern int drink_aff[][3];

// extern funcs
extern char *get_token(char *, char*);
extern int modify_target(struct char_data *ch);
extern int return_general(int skill_num);
extern bool check_quest_delivery(struct char_data *ch, struct char_data *mob, struct obj_data *obj);
extern void check_quest_delivery(struct char_data *ch, struct obj_data *obj);
void calc_weight(struct char_data *ch);

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
      act("$p won't fit in $P.", FALSE, ch, obj, cont, TO_CHAR);
      return;
    } if (GET_OBJ_TYPE(obj) == ITEM_HOLSTER || GET_OBJ_TYPE(obj) == ITEM_QUIVER)
      if (GET_OBJ_VAL(cont, 0)) {
        GET_OBJ_VAL(cont, 0)--;
        GET_OBJ_TIMER(obj) = 0;
      } else {
        act("$p won't fit in $P.", FALSE, ch, obj, cont, TO_CHAR);
        return;
      }
    else if (GET_OBJ_TYPE(obj) == ITEM_GUN_MAGAZINE)
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
    else if (GET_OBJ_TYPE(obj) == ITEM_WEAPON)
      if (GET_OBJ_VAL(obj, 3) == TYPE_HAND_GRENADE)
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
      else if (GET_OBJ_VAL(obj, 3) == TYPE_SHURIKEN)
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
      else {
        act("$p won't fit in $P.", FALSE, ch, obj, cont, TO_CHAR);
        return;
      }
    else if (GET_OBJ_VAL(cont, 4))
      if (GET_OBJ_WEIGHT(obj) <= 1) {
        GET_OBJ_VAL(cont, 4)--;
        GET_OBJ_TIMER(obj) = 4;
      } else {
        act("$p won't fit in $P.", FALSE, ch, obj, cont, TO_CHAR);
        return;
      }
    else {
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
  } else if (GET_OBJ_TYPE(cont) == ITEM_QUIVER)
  {
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
           || IS_OBJ_STAT( obj, ITEM_WIZLOAD) ) {
        sprintf(buf, "%s puts in (%ld) %s: (obj %ld) %s%s", GET_CHAR_NAME(ch),
                GET_OBJ_VNUM( cont ), cont->text.name,
                GET_OBJ_VNUM( obj ), obj->text.name,
                IS_OBJ_STAT(obj,ITEM_WIZLOAD) ? " *" : "");
        mudlog(buf, ch, IS_OBJ_STAT(obj, ITEM_WIZLOAD) ? LOG_WIZITEMLOG : LOG_CHEATLOG, TRUE);
      }
    }
  } else if (GET_OBJ_WEIGHT(cont) + GET_OBJ_WEIGHT(obj) > GET_OBJ_VAL(cont, 0))
    act("$p won't fit in $P.", FALSE, ch, obj, cont, TO_CHAR);
  else
  {
    if (obj->in_obj)
      obj_from_obj(obj);
    else
      obj_from_char(obj);
    obj_to_obj(obj, cont);
    act("You put $p in $P.", FALSE, ch, obj, cont, TO_CHAR);
    act("$n puts $p in $P.", TRUE, ch, obj, cont, TO_ROOM);
    if ( (!IS_NPC(ch) && access_level( ch, LVL_BUILDER ))
         || IS_OBJ_STAT( obj, ITEM_WIZLOAD) ) {
      sprintf(buf, "%s puts in (%ld) %s: (obj %ld) %s%s", GET_CHAR_NAME(ch),
              GET_OBJ_VNUM( cont ), cont->text.name,
              GET_OBJ_VNUM( obj ), obj->text.name,
              IS_OBJ_STAT(obj,ITEM_WIZLOAD) ? " *" : "");
      mudlog(buf, ch, IS_OBJ_STAT(obj, ITEM_WIZLOAD) ? LOG_WIZITEMLOG : LOG_CHEATLOG, TRUE);
    }
  }
}

void perform_put_cyberdeck(struct char_data * ch, struct obj_data * obj,
                           struct obj_data * cont)
{
  if (GET_OBJ_TYPE(cont) == ITEM_DECK_ACCESSORY)
  {
    if (GET_OBJ_VAL(cont, 0) != 2)
      send_to_char(ch, "You can't install anything into that.\r\n");
    else if (cont->carried_by)
      send_to_char(ch, "It doesn't seem to be switched on.\r\n");
    else if (GET_OBJ_TYPE(obj) != ITEM_DESIGN && GET_OBJ_TYPE(obj) != ITEM_PROGRAM && !(GET_OBJ_TYPE(obj) == ITEM_PROGRAM && GET_OBJ_TIMER(obj)))
      send_to_char(ch, "You can't install that onto a personal computer.\r\n");
    else if ((GET_OBJ_TYPE(obj) == ITEM_PROGRAM && (GET_OBJ_VAL(obj, 2) > GET_OBJ_VAL(cont, 2) - GET_OBJ_VAL(cont, 3))) ||
             (GET_OBJ_TYPE(obj) == ITEM_DESIGN && (GET_OBJ_VAL(obj, 6) + (GET_OBJ_VAL(obj, 6) / 10) > GET_OBJ_VAL(cont, 2) - GET_OBJ_VAL(cont, 3))))
      send_to_char(ch, "It doesn't seem to fit.\r\n");
    else {
      if (GET_OBJ_TYPE(obj) == ITEM_PROGRAM)
        GET_OBJ_VAL(cont, 3) += GET_OBJ_VAL(obj, 2);
      else
        GET_OBJ_VAL(cont, 3) += GET_OBJ_VAL(obj, 6) + (GET_OBJ_VAL(obj, 6) / 10);
      obj_from_char(obj);
      obj_to_obj(obj, cont);
      send_to_char(ch, "You install %s onto %s.\r\n", GET_OBJ_NAME(obj), GET_OBJ_NAME(cont));
    }
    return;
  } else if (GET_OBJ_TYPE(obj) == ITEM_DECK_ACCESSORY)
  {
    switch (GET_OBJ_VAL(obj, 0)) {
    case TYPE_FILE:
      if (GET_OBJ_VAL(cont, 5) + GET_OBJ_VAL(obj, 1) > GET_OBJ_VAL(cont, 3)) {
        act("$p takes up too much memory to be uploaded into $P.", FALSE, ch, obj, cont, TO_CHAR);
        return;
      }
      obj_from_char(obj);
      obj_to_obj(obj, cont);
      GET_OBJ_VAL(cont, 5) += GET_OBJ_VAL(obj, 2);
      act("You upload $p in $P.", FALSE, ch, obj, cont, TO_CHAR);
      return;
    case TYPE_UPGRADE:
      if (GET_OBJ_VAL(obj, 1) != 3) {
        send_to_char(ch, "You can't seem to fit this in.\r\n");
        return;
      }
      obj_from_char(obj);
      obj_to_obj(obj, cont);
      act("You fit $p into a FUP in $P.", FALSE, ch, obj, cont, TO_CHAR);
      return;
    default:
      send_to_char(ch, "You can't seem to fit this in.\r\n");
      return;
    }
  }
  if (GET_OBJ_VAL(cont, 0) == 0 || GET_OBJ_VAL(cont, 9))
    send_to_char("The cyberdeck doesn't respond.\r\n", ch);
  else if (!GET_OBJ_TIMER(obj) && GET_OBJ_VNUM(obj) == 108) 
    send_to_char("You can't install unburnt programs.\r\n", ch);
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
    send_to_char("You can't!\r\n", ch);
    return;
  }

  two_arguments(argument, arg1, arg2);
  obj_dotmode = find_all_dots(arg1);
  cont_dotmode = find_all_dots(arg2);
  cont_dotmode++;

  if (subcmd == SCMD_INSTALL)
    cyberdeck = TRUE;

  if (!*arg1) {
    sprintf(buf, "%s what in what?\r\n", (cyberdeck ? "Install" : "Put"));
    send_to_char(buf, ch);
  } else if (obj_dotmode != FIND_INDIV) {
    sprintf(buf, "You can only %s %s into one %s at a time.\r\n",
            (cyberdeck ? "install" : "put"), (cyberdeck ? "programs" : "things"),
            (cyberdeck ? "cyberdeck" : "container"));
    send_to_char(buf, ch);
  } else if (!*arg2) {
    sprintf(buf, "What do you want to %s %s in?\r\n", (cyberdeck ? "install" : "put"),
            ((obj_dotmode == FIND_INDIV) ? "it" : "them"));
    send_to_char(buf, ch);
  } else if (!str_cmp(arg2, "finger")) {
    for (cont = ch->cyberware; cont; cont = cont->next_content)
      if (GET_OBJ_VAL(cont, 0) == CYB_FINGERTIP)
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
    if ((GET_OBJ_TYPE(obj) != ITEM_PROGRAM && (GET_OBJ_TYPE(obj) == ITEM_DRUG &&
        GET_OBJ_VAL(obj, 0) != DRUG_CRAM && GET_OBJ_VAL(obj, 0) != DRUG_PSYCHE) && GET_OBJ_VNUM(obj) != 660)) {
      send_to_char("That doesn't fit in there.\r\n", ch);
      return;
    }
    obj_from_char(obj);
    obj_to_obj(obj, cont);
    send_to_char(ch, "You slip %s into your fingertip compartment.\r\n", GET_OBJ_NAME(obj));
    act("$n slips $o into $s fingertip compartment.\r\n", TRUE, ch, obj, 0, TO_ROOM);
  } else if (!str_cmp(arg2, "body")) {
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
      send_to_char("That doesn't fit in there.\r\n", ch);
      return;
    }
    obj_from_char(obj);
    obj_to_obj(obj, cont);
    send_to_char(ch, "You slip %s into your body compartment.\r\n", GET_OBJ_NAME(obj));
    act("$n slips $o into $s body compartment.\r\n", TRUE, ch, 0, obj, TO_ROOM);
  } else if (!str_cmp(arg2, "tooth")) {
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
    act("$n slips $o into a tooth compartment.\r\n", TRUE, ch, 0, obj, TO_ROOM);
  } else {
    generic_find(arg2, FIND_OBJ_EQUIP | FIND_OBJ_INV | FIND_OBJ_ROOM, ch, &tmp_char, &cont);
    if (!cont) {
      sprintf(buf, "You don't see %s %s here.\r\n", AN(arg2), arg2);
      send_to_char(buf, ch);
    } else if ((GET_OBJ_TYPE(cont) == ITEM_DECK_ACCESSORY && GET_OBJ_VAL(cont, 0) == TYPE_PARTS) ||
               (GET_OBJ_TYPE(cont) == ITEM_MAGIC_TOOL && GET_OBJ_VAL(cont, 0) == TYPE_SUMMONING)) {
      if (!(obj = get_obj_in_list_vis(ch, arg1, ch->carrying)))
        send_to_char(ch, "You aren't carrying %s %s.\r\n", AN(arg1), arg1);
      else if (obj == cont)
        send_to_char(ch, "You cannot combine an item with itself.\r\n");
      else if (GET_OBJ_TYPE(cont) != GET_OBJ_TYPE(obj) || GET_OBJ_VAL(obj, 0) != GET_OBJ_VAL(cont, 0))
        send_to_char(ch, "You cannot combine those two items.\r\n");
      else {
        GET_OBJ_COST(cont) += GET_OBJ_COST(obj);
        send_to_char(ch, "You combine %s and %s.\r\n", GET_OBJ_NAME(cont), GET_OBJ_NAME(obj));
        extract_obj(obj);
      }
    } else if ((!cyberdeck && (GET_OBJ_TYPE(cont) != ITEM_CONTAINER && GET_OBJ_TYPE(cont) != ITEM_QUIVER &&
                               GET_OBJ_TYPE(cont) != ITEM_WORN)) || (cyberdeck && !(GET_OBJ_TYPE(cont) ==
                                                                     ITEM_CYBERDECK || GET_OBJ_TYPE(cont) == ITEM_CUSTOM_DECK
                                                                     || GET_OBJ_TYPE(cont) == ITEM_DECK_ACCESSORY))) {
      sprintf(buf, "$p is not a %s.", (cyberdeck ? "cyberdeck" : "container"));
      act(buf, FALSE, ch, cont, 0, TO_CHAR);
    } else if (IS_SET(GET_OBJ_VAL(cont, 1), CONT_CLOSED) && (GET_OBJ_TYPE(cont) != ITEM_CYBERDECK
               && GET_OBJ_TYPE(cont) != ITEM_QUIVER && GET_OBJ_TYPE(cont) != ITEM_WORN && GET_OBJ_TYPE(cont) != ITEM_DECK_ACCESSORY) && GET_OBJ_TYPE(cont) != ITEM_CUSTOM_DECK)
      send_to_char("You'd better open it first!\r\n", ch);
    else {
      if (obj_dotmode == FIND_INDIV) {  /* put <obj> <container> */
        if (!(obj = get_obj_in_list_vis(ch, arg1, ch->carrying))) {
          sprintf(buf, "You aren't carrying %s %s.\r\n", AN(arg1), arg1);
          send_to_char(buf, ch);
        } else if ((obj == cont) && !cyberdeck)
          send_to_char("You attempt to fold it into itself, but fail.\r\n", ch);
        else if (cyberdeck && !((GET_OBJ_TYPE(obj) == ITEM_PROGRAM || GET_OBJ_TYPE(obj) == ITEM_DECK_ACCESSORY) || (GET_OBJ_TYPE(cont) == ITEM_DECK_ACCESSORY && GET_OBJ_TYPE(obj) == ITEM_DESIGN)))
          send_to_char("You can't install that in a cyberdeck!\r\n", ch);
        else if (GET_OBJ_TYPE(cont) == ITEM_QUIVER) {
          if (GET_OBJ_VAL(cont, 1) == 0 && !(GET_OBJ_TYPE(obj) == ITEM_MISSILE && GET_OBJ_VAL(obj, 0) == 0))
            send_to_char("Only arrows may be placed in that.\r\n", ch);
          else if (GET_OBJ_VAL(cont, 1) == 1 && !(GET_OBJ_TYPE(obj) == ITEM_MISSILE && GET_OBJ_VAL(obj, 0) == 1))
            send_to_char("Only bolts may be placed in that.\r\n", ch);
          else if (GET_OBJ_VAL(cont, 1) == 2 && !(GET_OBJ_TYPE(obj) == ITEM_WEAPON && GET_OBJ_VAL(obj, 3) == TYPE_SHURIKEN))
            send_to_char("Only shurikens can be stored in that.\r\n", ch);
          else if (GET_OBJ_VAL(cont, 1) == 3 && !(GET_OBJ_TYPE(obj) == ITEM_WEAPON && GET_OBJ_VAL(obj, 3) == TYPE_THROWING_KNIFE))
            send_to_char("That is used to hold throwing knives only.\r\n", ch);
          else {
            perform_put(ch, obj, cont);
          }
        } else if (!cyberdeck) {
          perform_put(ch, obj, cont);
        } else
          perform_put_cyberdeck(ch, obj, cont);
      } else {
        for (obj = ch->carrying; obj; obj = next_obj) {
          next_obj = obj->next_content;
          if (obj != cont && CAN_SEE_OBJ(ch, obj) &&
              (obj_dotmode == FIND_ALL || isname(arg1, obj->text.keywords))) {
            found = 1;
            if (!cyberdeck) {
              perform_put(ch, obj, cont);
            } else if (GET_OBJ_TYPE(obj) != ITEM_PROGRAM)
              send_to_char("That's not a program!\r\n", ch);
            else
              perform_put_cyberdeck(ch, obj, cont);
          }
        }
        if (!found) {
          if (obj_dotmode == FIND_ALL) {
            sprintf(buf, "You don't seem to have anything to %s in it.\r\n", (cyberdeck ? "install" : "put"));
            send_to_char(buf, ch);
          } else {
            sprintf(buf, "You don't seem to have any %ss.\r\n", arg1);
            send_to_char(buf, ch);
          }
        }
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
  } else if ((IS_CARRYING_W(ch) + GET_OBJ_WEIGHT(obj)) >
             CAN_CARRY_W(ch))
  {
    act("$p: you can't carry that much weight.", FALSE, ch, obj, 0, TO_CHAR);
    return 0;
  } else if (!(CAN_WEAR(obj, ITEM_WEAR_TAKE)) && !ch->in_veh)
  {
    act("$p: you can't take that!", FALSE, ch, obj, 0, TO_CHAR);
    return 0;
  }
  return 1;
}

void get_check_money(struct char_data * ch, struct obj_data * obj)
{
  int zone;

  if (GET_OBJ_TYPE(obj) == ITEM_MONEY)
  {
    for (zone = 0; zone <= top_of_zone_table; zone++)
      if (!zone_table[zone].connected && GET_OBJ_VNUM(obj) >= (zone_table[zone].number * 100) &&
          GET_OBJ_VNUM(obj) <= zone_table[zone].top)
        break;
    if (zone <= top_of_zone_table)
      act("$p dissolves in your hands!", FALSE, ch, obj, 0, TO_CHAR);
    else if (GET_OBJ_VAL(obj, 0) > (!GET_OBJ_VAL(obj, 1) ? 0 : -1)) {
      if (!GET_OBJ_VAL(obj, 1)) {  //paper money
        if (GET_OBJ_VAL(obj, 0) > 1)
          send_to_char(ch, "There were %d nuyen.\r\n", GET_OBJ_VAL(obj, 0));
        else
          send_to_char(ch, "There was 1 nuyen.\r\n");
        GET_NUYEN(ch) += GET_OBJ_VAL(obj, 0);
      } else
        return;
    } else {
      act("$p dissloves in your hands!", FALSE, ch, obj, 0, TO_CHAR);
      mudlog("ERROR: Nuyen value < 1", ch, LOG_SYSLOG, TRUE);
    }
    extract_obj(obj);
  }
}

void calc_weight(struct char_data *ch)
{
  struct obj_data *obj;
  int i=0;
  /* first reset the player carry weight*/
  IS_CARRYING_W(ch) = 0;
  /* Go Through worn eq*/

  for (i = 0; i < NUM_WEARS; i++)
    if (GET_EQ(ch, i))
      IS_CARRYING_W(ch) += GET_OBJ_WEIGHT(GET_EQ(ch, i));

  for (obj = ch->carrying; obj; obj = obj->next_content)
    IS_CARRYING_W(ch) +=GET_OBJ_WEIGHT(obj);
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

  if (mode == FIND_OBJ_INV || ch == cont->carried_by || ch == cont->worn_by || can_take_obj(ch, obj))
  {
    if (IS_CARRYING_N(ch) >= CAN_CARRY_N(ch))
      act("$p: you can't hold any more items.", FALSE, ch, obj, 0, TO_CHAR);
    else {
      if ( (!IS_NPC(ch) && access_level( ch, LVL_BUILDER ))
           || IS_OBJ_STAT( obj, ITEM_WIZLOAD) ) {
        sprintf(buf, "%s gets from (%ld) %s: (obj %ld) %s%s", GET_CHAR_NAME(ch),
                GET_OBJ_VNUM( cont ), cont->text.name,
                GET_OBJ_VNUM( obj ), obj->text.name,
                IS_OBJ_STAT(obj,ITEM_WIZLOAD) ? " *" : "");
        mudlog(buf, ch, IS_OBJ_STAT(obj, ITEM_WIZLOAD) ? LOG_WIZITEMLOG : LOG_CHEATLOG, TRUE);
      }

      if (GET_OBJ_TYPE(cont) == ITEM_QUIVER)
        GET_OBJ_VAL(cont, 2) = MAX(0, GET_OBJ_VAL(cont, 2) - 1);
      sprintf(buf, "You %s $p from $P.", (cyberdeck || computer ? "uninstall" : "get"));
      if (computer) {
        for (struct char_data *vict = world[ch->in_room].people; vict; vict = vict->next_in_room)
          if ((AFF_FLAGGED(vict, AFF_PROGRAM) || AFF_FLAGGED(vict, AFF_DESIGN)) && vict != ch) {
            send_to_char(ch, "You can't uninstall that while someone is working on it.\r\n");
            return;
          } else if (vict == ch && vict->char_specials.programming == obj) {
            send_to_char(ch, "You stop %sing %s.\r\n", AFF_FLAGGED(ch, AFF_PROGRAM) ? "programm" : "design", GET_OBJ_NAME(obj));
            STOP_WORKING(ch);
            break;
          }
        if (GET_OBJ_TYPE(obj) == ITEM_PROGRAM)
          GET_OBJ_VAL(cont, 3) -= GET_OBJ_VAL(obj, 2);
        else
          GET_OBJ_VAL(cont, 3) -= GET_OBJ_VAL(obj, 6) + (GET_OBJ_VAL(obj, 6) / 10);
      } else if (cyberdeck) {
        if (GET_OBJ_TYPE(obj) == ITEM_PROGRAM && (GET_OBJ_VAL(cont, 0) == 0 || GET_OBJ_VAL(cont, 9))) {
          send_to_char("The cyberdeck is unresponsive.\r\n", ch);
          return;
        }
        if (GET_OBJ_TYPE(obj) == ITEM_PROGRAM ||
            (GET_OBJ_TYPE(obj) == ITEM_DECK_ACCESSORY && GET_OBJ_VAL(obj, 0) == TYPE_FILE))
          GET_OBJ_VAL(cont, 5) -= GET_OBJ_VAL(obj, 2);
        if (GET_OBJ_TYPE(obj) == ITEM_PART) {
          if (GET_OBJ_VAL(obj, 0) == PART_STORAGE) {
            for (struct obj_data *k = cont->contains; k; k = k->next_content)
              if ((GET_OBJ_TYPE(k) == ITEM_DECK_ACCESSORY && GET_OBJ_VAL(k, 0) == TYPE_FILE) ||
                  GET_OBJ_TYPE(k) == ITEM_PROGRAM) {
                send_to_char("You cannot uninstall that while you have files installed.\r\n", ch);
                return;
              }
            GET_OBJ_VAL(cont, 5) = GET_OBJ_VAL(cont, 3) = 0;
          }
          switch(GET_OBJ_VAL(obj, 0)) {
          case PART_MPCP:
          case PART_ACTIVE:
          case PART_BOD:
          case PART_SENSOR:
          case PART_IO:
          case PART_MATRIX_INTERFACE:
            GET_OBJ_VAL(cont, 9) = 1;
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
      obj_from_obj(obj);
      obj_to_char(obj, ch);
      get_check_money(ch, obj);
    }
  }
}


void get_from_container(struct char_data * ch, struct obj_data * cont,
                        char *arg, int mode)
{
  struct obj_data *obj, *next_obj;
  int obj_dotmode, found = 0;

  obj_dotmode = find_all_dots(arg);

  if (GET_OBJ_TYPE(cont) == ITEM_CYBERDECK || GET_OBJ_TYPE(cont) == ITEM_CUSTOM_DECK)
  {
    if (obj_dotmode == FIND_ALL || obj_dotmode == FIND_ALLDOT) {
      send_to_char("You can't uninstall more than one program at a time!\r\n", ch);
      return;
    }
    obj = get_obj_in_list_vis(ch, arg, cont->contains);
    if (!obj) {
      sprintf(buf, "There doesn't seem to be %s %s installed in $p.", AN(arg), arg);
      act(buf, FALSE, ch, cont, 0, TO_CHAR);
      return;
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
  if ( IS_OBJ_STAT(cont, ITEM_CORPSE) )
  {
    if (GET_OBJ_VAL(cont, 4) == 1 && GET_OBJ_VAL(cont, 5) != GET_IDNUM(ch)
         && !IS_SENATOR(ch)) {
      send_to_char("You cannot loot this corpse.\n\r",ch);
      return;
/*    } else if (GET_OBJ_VAL(cont, 4) == 2 && !(PRF_FLAGGED(ch, PRF_PKER)
               || GET_IDNUM(ch) == GET_OBJ_VAL(cont, 5)) && !IS_SENATOR(ch)) {
      send_to_char("You cannot loot this corpse.\r\n", ch);
      return;*/
    } else {
      if (obj_dotmode == FIND_INDIV) {
        if (!(obj = get_obj_in_list_vis(ch, arg, cont->contains))) {
          sprintf(buf, "There doesn't seem to be %s %s in $p.", AN(arg), arg);
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
            sprintf(buf, "You can't seem to find any %ss in $p.", arg);
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
      sprintf(buf, "There doesn't seem to be %s %s in $p.", AN(arg), arg);
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
        sprintf(buf, "You can't seem to find any %ss in $p.", arg);
        act(buf, FALSE, ch, cont, 0, TO_CHAR);
      }
    }
  }
}

int perform_get_from_room(struct char_data * ch, struct obj_data * obj, bool download)
{
  if (GET_OBJ_TYPE(obj) == ITEM_DECK_ACCESSORY)
    switch (GET_OBJ_VAL(obj, 0))
    {
    case TYPE_COMPUTER:
      for (struct char_data *vict = ch->in_veh ? ch->in_veh->people : world[ch->in_room].people; vict; vict = ch->in_veh ? vict->next_in_veh : vict->next_in_room)
        if (vict->char_specials.programming && vict->char_specials.programming->in_obj == obj) {
          if (vict == ch)
            send_to_char(ch, "You are using that already.\r\n");
          else
            act("$N seems to be using $p.", FALSE, ch, obj, vict, TO_CHAR);
          return FALSE;
        }
      break;
    case TYPE_COOKER:
      if (GET_OBJ_VAL(obj, 9)) {
        send_to_char(ch, "It is in the middle of encoding a chip, leave it alone.\r\n");
        return FALSE;
      }
      break;
    }
  if (GET_OBJ_TYPE(obj) == ITEM_WORKSHOP && GET_OBJ_VAL(obj, 1) > 1 && (GET_OBJ_VAL(obj, 2) || GET_OBJ_VAL(obj, 3)))
    send_to_char(ch, "You may wish to pack it up first.\r\n");
  else if (can_take_obj(ch, obj))
  {
    if (GET_OBJ_TYPE(obj) == ITEM_WORKSHOP)
      for (struct char_data *tmp = ch->in_veh ? ch->in_veh->people : world[ch->in_room].people; tmp; tmp = ch->in_veh ? tmp->next_in_veh : tmp->next_in_room)
         if (AFF_FLAGGED(tmp, AFF_PACKING)) {
           send_to_char("Someone is working on that workshop.\r\n", ch);
           return FALSE;
         }
    if ( (!IS_NPC(ch) && access_level( ch, LVL_BUILDER ))
         || IS_OBJ_STAT( obj, ITEM_WIZLOAD) ) {
      sprintf(buf, "%s gets from room: (obj %ld) %s%s", GET_CHAR_NAME(ch),
              GET_OBJ_VNUM( obj ), obj->text.name,
              IS_OBJ_STAT(obj,ITEM_WIZLOAD) ? " *" : "");
      mudlog(buf, ch, IS_OBJ_STAT(obj, ITEM_WIZLOAD) ? LOG_WIZITEMLOG : LOG_CHEATLOG, TRUE);
    }
    obj_from_room(obj);
    obj_to_char(obj, ch);
    act("You get $p.", FALSE, ch, obj, 0, TO_CHAR);
    if (ch->in_veh) {
      sprintf(buf, "%s gets %s.\r\n", GET_NAME(ch), GET_OBJ_NAME(obj));
      send_to_veh(buf, ch->in_veh, ch, FALSE);
    } else
      act("$n gets $p.", FALSE, ch, obj, 0, TO_ROOM);
    get_check_money(ch, obj);
    affect_total(ch);
    return 1;
  }
  return 0;
}

void get_from_room(struct char_data * ch, char *arg, bool download)
{
  struct obj_data *obj, *next_obj;
  int dotmode, found = 0;
  dotmode = find_all_dots(arg);

  if (dotmode == FIND_INDIV || download)
  {
    if (ch->in_veh)
      obj = get_obj_in_list_vis(ch, arg, ch->in_veh->contents);
    else
      obj = get_obj_in_list_vis(ch, arg, world[ch->in_room].contents);
    if (!obj) {
      sprintf(buf, "You don't see %s %s here.\r\n", AN(arg), arg);
      send_to_char(buf, ch);
    } else {
      if ( CAN_SEE_OBJ(ch, obj) ) {
        if ( IS_OBJ_STAT(obj, ITEM_CORPSE) && GET_OBJ_VAL(obj, 4) == 1
             && GET_OBJ_VAL(obj, 5) != GET_IDNUM(ch) && !IS_SENATOR(ch) )
          send_to_char("It's not yours chummer...better leave it be.\n\r",ch);
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
      obj = world[ch->in_room].contents;

    for (;obj; obj = next_obj) {
      next_obj = obj->next_content;
      if (ch->in_veh && ch->vfront != obj->vfront)
        continue;
      if (CAN_SEE_OBJ(ch, obj) &&
          (dotmode == FIND_ALL || isname(arg, obj->text.keywords))) {
        found = 1;
        if ( IS_OBJ_STAT(obj, ITEM_CORPSE) && GET_OBJ_VAL(obj, 4) == 1
             && GET_OBJ_VAL(obj, 5) != GET_IDNUM(ch) && !access_level(ch, LVL_FIXER) )
          send_to_char("It's not yours chummer...better leave it be.\n\r",ch);
        else {
          perform_get_from_room(ch, obj, FALSE);
        }
      }
    }
    if (!found) {
      if (dotmode == FIND_ALL)
        send_to_char("There doesn't seem to be anything here.\r\n", ch);
      else {
        sprintf(buf, "You don't see any %ss here.\r\n", arg);
        send_to_char(buf, ch);
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
  two_arguments(argument, arg1, arg2);

  if (IS_CARRYING_N(ch) >= CAN_CARRY_N(ch))
    send_to_char("Your arms are already full!\r\n", ch);
  else if (!*arg1) {
    sprintf(buf, "%s what?\r\n", (cyberdeck ? "Uninstall" : (download ? "Download" : "Get")));
    send_to_char(buf, ch);
  } else if (!str_cmp(arg1, "tooth")) {
    for (cont = ch->cyberware; cont; cont = cont->next_content)
      if (GET_OBJ_VAL(cont, 0) == CYB_TOOTHCOMPARTMENT)
        break;
    if (!cont)
      send_to_char("You don't have a tooth compartment.\r\n", ch);
    else if (!cont->contains)
      send_to_char("There's nothing in there.\r\n", ch);
    else if (!success_test(GET_QUI(ch), 4))
      send_to_char("You can't seem to get it out.\r\n", ch);
    else {
      obj = cont->contains;
      obj_from_obj(obj);
      obj_to_char(obj, ch);
      send_to_char(ch, "You remove %s from your tooth compartment.\r\n", GET_OBJ_NAME(obj));
      act("$n reaches into $s mouth and removes a $o.", TRUE, ch, 0, obj, TO_ROOM);
    }
  } else if (!str_cmp(arg1, "finger")) {
    for (cont = ch->cyberware; cont; cont = cont->next_content)
      if (GET_OBJ_VAL(cont, 0) == CYB_FINGERTIP)
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
      act("$n removes $O from a fingertip compartment.", TRUE, ch, 0, obj, TO_ROOM);
    }
  } else if (!str_cmp(arg1, "body")) {
    for (cont = ch->cyberware; cont; cont = cont->next_content)
      if (GET_OBJ_VAL(cont, 0) == CYB_BODYCOMPART)
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
      act("$n removes $o from a body compartment.", TRUE, ch, 0, obj, TO_ROOM);
    }
  } else if (!*arg2 || download) {
    get_from_room(ch, arg1, download);
  } else {
    cont_dotmode = find_all_dots(arg2);
    if (cont_dotmode == FIND_INDIV) {
      mode = generic_find(arg2, FIND_OBJ_EQUIP | FIND_OBJ_INV | FIND_OBJ_ROOM, ch, &tmp_char, &cont);
      if (!ch->in_veh || (ch->in_veh->flags.IsSet(VFLAG_WORKSHOP) && !ch->vfront))
        veh = get_veh_list(arg2, ch->in_veh ? ch->in_veh->carriedvehs : world[ch->in_room].vehicles, ch);
      if (cyberdeck && veh) {
        cont = NULL;
        if (veh->owner != GET_IDNUM(ch) && veh->locked) {
          sprintf(buf, "%s anti-theft measures beep loudly.\r\n", GET_VEH_NAME(veh));
          act(buf, FALSE, ch, 0, 0, TO_ROOM);
          send_to_char(buf, ch);
          return;
        }
        if (veh->cspeed > SPEED_OFF) {
          send_to_char("It's moving a little fast...\r\n", ch);
          return;
        }
        for (found = 0; found < NUM_MODS; found++)
          if (GET_MOD(veh, found))
            if (isname(arg1, GET_MOD(veh, found)->text.name)) {
              cont = GET_MOD(veh, found);
              break;
            }
        if (!cont && veh->mount)
          for (obj = veh->mount; obj; obj = obj->next_content)
            if (isname(arg1, obj->text.name)) {
              cont = obj;
              cont_dotmode = 1;
              break;
            }
        if (!cont) {
          sprintf(buf, "There doesn't seem to be a %s installed on %s.\r\n", arg1, GET_VEH_NAME(veh));
          send_to_char(buf, ch);
          return;
        } else {
          if (!IS_NPC(ch)) {
            switch(veh->type) {
            case VEH_DRONE:
              skill = SKILL_BR_DRONE;
              break;
            case VEH_BIKE:
              skill = SKILL_BR_BIKE;
              break;
            case VEH_CAR:
              skill = SKILL_BR_CAR;
              break;
            case VEH_TRUCK:
              skill = SKILL_BR_TRUCK;
              break;
            }
            switch (GET_OBJ_VAL(cont, 0)) {
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
            case TYPE_ARMOUR:
            case TYPE_CONCEALEDARMOUR:
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
              if (kit == TYPE_SHOP)
                target--;
              else if (kit == TYPE_FACILITY)
                target -= 3;
            } else if (mod_types[GET_OBJ_VAL(cont, 0)].tools == TYPE_SHOP && kit == TYPE_FACILITY)
              target--;
            if (GET_OBJ_VAL(cont, 0) == TYPE_ENGINECUST)
              veh->engine = 0;
            if (success_test(skill, target) < 1) {
              send_to_char(ch, "You can't figure out how to uninstall it. \r\n");
              return;
            }
          }
          sprintf(buf, "$n goes to work on %s.", GET_VEH_NAME(veh));
          sprintf(buf2, "You go to work on %s and remove %s.\r\n", GET_VEH_NAME(veh), GET_OBJ_NAME(cont));
          send_to_char(buf2, ch);
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
            case 0:
              bod++;
              load = 10;
              break;
            case 3:
              sig = 1;
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
            veh->usedload -= GET_OBJ_VAL(cont, 1);
            GET_MOD(veh, found) = NULL;
            for (found = 0; found < MAX_OBJ_AFFECT; found++)
              if (cont->affected[found].location == VAFF_SEN || cont->affected[found].location == VAFF_AUTO ||
                  cont->affected[found].location == VAFF_PILOT)
                affect_veh(veh, cont->affected[found].location, 0);
              else
                affect_veh(veh, cont->affected[found].location, -(cont->affected[found].modifier));
          }
          obj_to_char(cont, ch);
          return;
        }
      } else if (!cont) {
        sprintf(buf, "You don't have %s %s.\r\n", AN(arg2), arg2);
        send_to_char(buf, ch);
      } else if ((!cyberdeck && !(GET_OBJ_TYPE(cont) == ITEM_CONTAINER || GET_OBJ_TYPE(cont) ==
                                  ITEM_QUIVER || GET_OBJ_TYPE(cont) == ITEM_HOLSTER || GET_OBJ_TYPE(cont) ==
                                  ITEM_WORN)) || (cyberdeck && !(GET_OBJ_TYPE(cont) == ITEM_CYBERDECK ||
                                                                 GET_OBJ_TYPE(cont) == ITEM_CUSTOM_DECK ||
                                                                 GET_OBJ_TYPE(cont) == ITEM_DECK_ACCESSORY))) {
        sprintf(buf, "$p is not a %s", (!cyberdeck ? "container" : "cyberdeck"));
        act(buf, FALSE, ch, cont, 0, TO_CHAR);
      } else {
        get_from_container(ch, cont, arg1, mode);
      }
    } else {
      if (cont_dotmode == FIND_ALLDOT && !*arg2) {
        send_to_char("Get from all of what?\r\n", ch);
        return;
      }
      for (cont = ch->carrying; cont; cont = cont->next_content)
        if (CAN_SEE_OBJ(ch, cont) &&
            (cont_dotmode == FIND_ALL || isname(arg2, cont->text.keywords))) {
          if ((!cyberdeck && GET_OBJ_TYPE(cont) == ITEM_CONTAINER) ||
              (cyberdeck && (GET_OBJ_TYPE(cont) == ITEM_CYBERDECK || GET_OBJ_TYPE(cont) == ITEM_CUSTOM_DECK))) {
            found = 1;
            get_from_container(ch, cont, arg1, FIND_OBJ_INV);
          } else if (cont_dotmode == FIND_ALLDOT) {
            found = 1;
            sprintf(buf, "$p is not a %s", (!cyberdeck ? "container" : "cyberdeck"));
            act(buf, FALSE, ch, cont, 0, TO_CHAR);
          }
        }
      for (cont = world[ch->in_room].contents; cont; cont = cont->next_content)
        if (CAN_SEE_OBJ(ch, cont) &&
            (cont_dotmode == FIND_ALL || isname(arg2, cont->text.keywords))) {
          if (GET_OBJ_TYPE(cont) == ITEM_CONTAINER) {
            get_from_container(ch, cont, arg1, FIND_OBJ_ROOM);
            found = 1;
          } else if (cont_dotmode == FIND_ALLDOT) {
            sprintf(buf, "$p is not a %s", (!cyberdeck ? "container" : "cyberdeck"));
            act(buf, FALSE, ch, cont, 0, TO_CHAR);
            found = 1;
          }
        }
      if (!found) {
        if (cont_dotmode == FIND_ALL) {
          sprintf(buf, "You can't seem to find any %s.\r\n", (!cyberdeck ? "containers" : "cyberdeck"));
          send_to_char(buf, ch);
        } else {
          sprintf(buf, "You can't seem to find any %ss here.\r\n", arg2);
          send_to_char(buf, ch);
        }
      }
    }
  }
}

void perform_drop_gold(struct char_data * ch, int amount, byte mode, vnum_t RDR)
{
  struct obj_data *obj;

  if (mode != SCMD_DROP)
  {
    send_to_char("You can't do that!\r\n", ch);
    return;
  } else if (amount < 1)
  {
    send_to_char("How does that work?\r\n", ch);
    return;
  } else if (amount > GET_NUYEN(ch))
  {
    send_to_char("You don't even have that much!\r\n", ch);
    return;
  }

  obj = read_object(110, VIRTUAL);
  GET_OBJ_VAL(obj, 0) = amount;

  if ( !IS_NPC(ch) && (access_level(ch, LVL_BUILDER)
       || IS_NPC(ch)))
    obj->obj_flags.extra_flags.SetBit(ITEM_WIZLOAD);

  GET_NUYEN(ch) -= amount;
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

    sprintf(buf, "%s drops: %d nuyen *", GET_CHAR_NAME(ch),
            amount);
    mudlog(buf, ch, LOG_CHEATLOG, TRUE);
  }

  return;
}

#define VANISH(mode) ((mode == SCMD_DONATE || mode == SCMD_JUNK) ? "  It vanishes into a recycling bin!" : "")

int perform_drop(struct char_data * ch, struct obj_data * obj, byte mode,
                 const char *sname, vnum_t RDR)
{
  int value;

  if (IS_OBJ_STAT(obj, ITEM_NODROP))
  {
    sprintf(buf, "You can't %s $p, it must be CURSED!", sname);
    act(buf, FALSE, ch, obj, 0, TO_CHAR);
    return 0;
  }
  if (obj == ch->char_specials.programming)
  {
    send_to_char(ch, "You can't %s something you are working on.", sname);
    return 0;
  }
  if (ch->in_veh)
  {
    if (ch->in_veh->usedload + GET_OBJ_WEIGHT(obj) > ch->in_veh->load) {
      send_to_char("There is too much in the vehicle already!\r\n", ch);
      return 0;
    }
    if (ch->vfront && ch->in_veh->seating[0] && ch->in_veh->usedload + GET_OBJ_WEIGHT(obj) > ch->in_veh->load / 10) {
      send_to_char("There is too much in the front of the vehicle!\r\n", ch);
      return 0;
    }
    sprintf(buf, "%s %ss %s.%s\r\n", GET_NAME(ch), sname, GET_OBJ_NAME(obj), VANISH(mode));
    send_to_veh(buf, ch->in_veh, ch, FALSE);
  } else
  {
    sprintf(buf, "$n %ss $p.%s", sname, VANISH(mode));
    act(buf, TRUE, ch, obj, 0, TO_ROOM);
  }

  sprintf(buf, "You %s $p.%s", sname, VANISH(mode));
  act(buf, FALSE, ch, obj, 0, TO_CHAR);
  if (obj->in_obj)
    obj_from_obj(obj);
  else
    obj_from_char(obj);
  affect_total(ch);

  if ((mode == SCMD_DONATE) && IS_OBJ_STAT(obj, ITEM_NODONATE))
    mode = SCMD_JUNK;

  if ( (!IS_NPC(ch) && access_level( ch, LVL_BUILDER ))
       || IS_OBJ_STAT( obj, ITEM_WIZLOAD) )
  {
    sprintf(buf, "%s %ss: (obj %ld) %s%s", GET_CHAR_NAME(ch),
            sname,
            GET_OBJ_VNUM( obj ), obj->text.name,
            IS_OBJ_STAT(obj,ITEM_WIZLOAD) ? " *" : "");
    mudlog(buf, ch, IS_OBJ_STAT(obj, ITEM_WIZLOAD) ? LOG_WIZITEMLOG : LOG_CHEATLOG, TRUE);
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
    obj_to_room(obj, RDR);
    if (world[RDR].people)
      act("You notice $p exposed beneath the junk.", FALSE, world[RDR].people, obj, 0, TO_ROOM);
    return 0;
  case SCMD_JUNK:
    value = MAX(1, MIN(200, GET_OBJ_COST(obj) >> 4));
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
    send_to_char("You can't!\r\n", ch);
    return;
  }
  if (AFF_FLAGGED(ch, AFF_PILOT)) {
    send_to_char("Now that would be a good trick!\r\n", ch);
    return;
  }

  if (PLR_FLAGGED(ch, PLR_AUTH) && (subcmd == SCMD_DROP || subcmd == SCMD_DONATE)) {
    send_to_char(ch, "You cannot drop items until you are authed.\r\n");
    return;
  } else if (IS_WORKING(ch)) {
    send_to_char(TOOBUSY, ch);
    return;
  }

  struct obj_data *obj, *next_obj;
  rnum_t RDR = 0;
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
      RDR = real_room(donation_room_1);
      break;
    case 5:
    case 2:
      RDR = real_room(donation_room_2);
      break;
    case 4:
    case 3:
      RDR = real_room(donation_room_3);
      break;
    }
    if (RDR == NOWHERE) {
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
    sprintf(buf, "What do you want to %s?\r\n", sname);
    send_to_char(buf, ch);
    return;
  } else if (is_number(arg)) {
    amount = atoi(arg);
    argument = one_argument(argument, arg);
    if (!str_cmp("nuyen", arg))
      perform_drop_gold(ch, amount, mode, RDR);
    else {
      /* code to drop multiple items.  anyone want to write it? -je */
      send_to_char("Sorry, you can't do that to more than one item at a time.\r\n", ch);
    }
    return;
  } else {
    dotmode = find_all_dots(arg);

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
          amount += perform_drop(ch, obj, mode, sname, RDR);
        }
    } else if (dotmode == FIND_ALLDOT) {
      if (!*arg) {
        sprintf(buf, "What do you want to %s all of?\r\n", sname);
        send_to_char(buf, ch);
        return;
      }
      if (!(obj = get_obj_in_list_vis(ch, arg, ch->carrying))) {
        sprintf(buf, "You don't seem to have any %ss.\r\n", arg);
        send_to_char(buf, ch);
      }
      while (obj) {
        next_obj = get_obj_in_list_vis(ch, arg, obj->next_content);
        amount += perform_drop(ch, obj, mode, sname, RDR);
        obj = next_obj;
      }
    } else {
      if (!(obj = get_obj_in_list_vis(ch, arg, ch->carrying))) {
        sprintf(buf, "You don't seem to have %s %s.\r\n", AN(arg), arg);
        send_to_char(buf, ch);
      } else
        amount += perform_drop(ch, obj, mode, sname, RDR);
    }
  }
  if (amount && (subcmd == SCMD_JUNK)) {
    send_to_char(ch, "You receive %d nuyen for recycling.\r\n", amount >> 4);
    GET_NUYEN(ch) += amount >> 4;
  }
}

bool perform_give(struct char_data * ch, struct char_data * vict, struct obj_data * obj)
{
  if (IS_ASTRAL(vict))
  {
    act("What use would $E have for $p?!", FALSE, ch, obj, vict, TO_CHAR);
    return 0;
  }
  if (IS_OBJ_STAT(obj, ITEM_NODROP))
  {
    act("You can't let go of $p!!  Yeech!", FALSE, ch, obj, 0, TO_CHAR);
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
  obj_from_char(obj);
  obj_to_char(obj, vict);
  act("You give $p to $N.", FALSE, ch, obj, vict, TO_CHAR);
  act("$n gives you $p.", FALSE, ch, obj, vict, TO_VICT);
  if (ch->in_veh)
  {
    sprintf(buf, "%s gives %s to %s.\r\n", GET_NAME(ch), GET_OBJ_NAME(obj), GET_NAME(vict));
    send_to_veh(buf, ch->in_veh, ch, vict, FALSE);
  }
  act("$n gives $p to $N.", TRUE, ch, obj, vict, TO_NOTVICT);

  if ( (!IS_NPC(ch) && access_level( ch, LVL_BUILDER ))
       || IS_OBJ_STAT( obj, ITEM_WIZLOAD) )
  {
    sprintf(buf, "%s gives %s: (obj %ld) %s%s",
            GET_CHAR_NAME(ch), GET_CHAR_NAME(vict),
            GET_OBJ_VNUM( obj ), obj->text.name,
            IS_OBJ_STAT(obj,ITEM_WIZLOAD) ? " *" : "");
    mudlog(buf, ch, IS_OBJ_STAT(obj, ITEM_WIZLOAD) ? LOG_WIZITEMLOG : LOG_CHEATLOG, TRUE);
  }

  if (!IS_NPC(ch) && IS_NPC(vict) && GET_QUEST(ch)) {
    if (check_quest_delivery(ch, vict, obj))
      extract_obj(obj);
  } else if (AFF_FLAGGED(ch, AFF_GROUP) && ch->master &&
           !IS_NPC(ch->master) && IS_NPC(vict) && GET_QUEST(ch->master))
    if (check_quest_delivery(ch->master, vict, obj))
      extract_obj(obj);

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
    send_to_char(NOPERSON, ch);
    return NULL;
  } else if (vict == ch)
  {
    send_to_char("What's the point of that?\r\n", ch);
    return NULL;
  } else if (IS_NPC(vict) && GET_MOB_SPEC(vict) && GET_MOB_SPEC(vict) == fixer)
  {
    act("Do you really want to give $M stuff for free?!",
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
  if (IS_SENATOR(ch) && !access_level(ch, LVL_VICEPRES) && !IS_SENATOR(vict))
  {
    send_to_char("Maybe that's not such a good idea...\r\n", ch);
    return;
  }
  send_to_char(OK, ch);
  sprintf(buf, "$n gives you %d nuyen.", amount);
  act(buf, FALSE, ch, 0, vict, TO_VICT);
  if (ch->in_veh)
  {
    sprintf(buf, "%s gives some nuyen to %s.", GET_NAME(ch), GET_NAME(vict));
    send_to_veh(buf, ch->in_veh, ch, vict, FALSE);
  } else
  {
    sprintf(buf, "$n gives some nuyen to $N.");
    act(buf, TRUE, ch, 0, vict, TO_NOTVICT);
  }
  if (IS_NPC(ch) || !access_level(ch, LVL_VICEPRES))
    GET_NUYEN(ch) -= amount;
  GET_NUYEN(vict) += amount;

  sprintf(buf, "%s gives %s: %d nuyen *",
          GET_CHAR_NAME(ch), GET_CHAR_NAME(vict), amount);
  mudlog(buf, ch, LOG_GRIDLOG, TRUE);
}

ACMD(do_give)
{
  int amount, dotmode;
  struct char_data *vict;
  struct obj_data *obj, *next_obj;

  argument = one_argument(argument, arg);

  if (IS_ASTRAL(ch) || PLR_FLAGGED(ch, PLR_AUTH)) {
    send_to_char("You can't!\r\n", ch);
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
        vict = get_char_veh(ch, arg, ch->in_veh);
        if (vict) {
          perform_give_gold(ch, vict, amount);
          return;
        }
        send_to_char(NOPERSON, ch);
        return;
      }
      if ((vict = give_find_vict(ch, arg)))
        perform_give_gold(ch, vict, amount);
      return;
    } else {
      /* code to give multiple items.  anyone want to write it? -je */
      send_to_char("You can't give more than one item at a time.\r\n", ch);
      return;
    }
  } else {
    one_argument(argument, buf1);
    if (ch->in_veh) {
      vict = get_char_veh(ch, buf1, ch->in_veh);
      if (!vict) {
        send_to_char(NOPERSON, ch);
        return;
      }
    } else if (!(vict = give_find_vict(ch, buf1)))
      return;
    dotmode = find_all_dots(arg);
    if (dotmode == FIND_INDIV) {
      if (!(obj = get_obj_in_list_vis(ch, arg, ch->carrying))) {
        sprintf(buf, "You don't seem to have %s %s.\r\n", AN(arg), arg);
        send_to_char(buf, ch);
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
  struct obj_data *tmp_obj;
  struct char_data *tmp_ch;

  if (obj->in_room != NOWHERE)
  {
    GET_OBJ_WEIGHT(obj) = MAX(0, GET_OBJ_WEIGHT(obj) - weight);
  } else if ((tmp_ch = obj->carried_by))
  {
    obj_from_char(obj);
    GET_OBJ_WEIGHT(obj) += weight;
    obj_to_char(obj, tmp_ch);
  } else if ((tmp_obj = obj->in_obj))
  {
    obj_from_obj(obj);
    GET_OBJ_WEIGHT(obj) += weight;
    obj_to_obj(obj, tmp_obj);
  } else
  {
    log("SYSERR: Unknown attempt to subtract weight from an object.");
  }
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

  new_name = new char[strlen(obj->text.keywords)+strlen(drinknames[type])+2];
  sprintf(new_name, "%s %s", obj->text.keywords, drinknames[type]);

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
    if (!(temp = get_obj_in_list_vis(ch, arg, world[ch->in_room].contents))) {
      send_to_char(NOOBJECT, ch);
      return;
    } else
      on_ground = 1;
  }
  if ((GET_OBJ_TYPE(temp) != ITEM_DRINKCON) &&
      (GET_OBJ_TYPE(temp) != ITEM_FOUNTAIN)) {
    send_to_char("You can't drink from that!\r\n", ch);
    return;
  }
  if (on_ground && (GET_OBJ_TYPE(temp) == ITEM_DRINKCON)) {
    send_to_char("You have to be holding that to drink from it.\r\n", ch);
    return;
  }
  if ((GET_COND(ch, DRUNK) > 10)) {
    /* The pig is drunk */
    send_to_char("You can't seem to get close enough to your mouth.\r\n", ch);
    act("$n tries to drink but misses $s mouth!", TRUE, ch, 0, 0, TO_ROOM);
    return;
  }
  if ((GET_COND(ch, FULL) > 20) && (GET_COND(ch, THIRST) > 0)) {
    send_to_char("Your stomach can't contain anymore!\r\n", ch);
    return;
  }
  if (!GET_OBJ_VAL(temp, 1)) {
    send_to_char("It's empty.\r\n", ch);
    return;
  }
  if (subcmd == SCMD_DRINK) {
    sprintf(buf, "$n drinks %s from $p.", drinknames[GET_OBJ_VAL(temp, 2)]);
    act(buf, TRUE, ch, temp, 0, TO_ROOM);

    sprintf(buf, "You drink the %s.\r\n", drinknames[GET_OBJ_VAL(temp, 2)]);
    send_to_char(buf, ch);
    amount = number(3, 10);
  } else {
    act("$n sips from $p.", TRUE, ch, temp, 0, TO_ROOM);
    sprintf(buf, "It tastes like %s.\r\n", drinknames[GET_OBJ_VAL(temp, 2)]);
    send_to_char(buf, ch);
    amount = 1;
  }

  amount = MIN(amount, GET_OBJ_VAL(temp, 1));

  /* You can't subtract more than the object weighs */
  weight = (float)(MIN(amount * 100, (int)(GET_OBJ_WEIGHT(temp) * 100)) / 100);

  weight_change_object(temp, -weight);  /* Subtract amount */

  gain_condition(ch, DRUNK,
                 (int) ((int) drink_aff[GET_OBJ_VAL(temp, 2)][DRUNK] * amount) / 4);

  gain_condition(ch, FULL,
                 (int) ((int) drink_aff[GET_OBJ_VAL(temp, 2)][FULL] * amount) / 4);

  gain_condition(ch, THIRST,
                 (int) ((int) drink_aff[GET_OBJ_VAL(temp, 2)][THIRST] * amount) / 4);

  if (GET_COND(ch, DRUNK) > 10)
    send_to_char("You feel drunk.\r\n", ch);

  if (GET_COND(ch, THIRST) > 20)
    send_to_char("You don't feel thirsty any more.\r\n", ch);

  if (GET_COND(ch, FULL) > 20)
    send_to_char("You are full.\r\n", ch);

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
    sprintf(buf, "You don't seem to have %s %s.\r\n", AN(arg), arg);
    send_to_char(buf, ch);
    return;
  }
  if (subcmd == SCMD_TASTE && ((GET_OBJ_TYPE(food) == ITEM_DRINKCON) ||
                               (GET_OBJ_TYPE(food) == ITEM_FOUNTAIN))) {
    do_drink(ch, argument, 0, SCMD_SIP);
    return;
  }
  if ((GET_OBJ_TYPE(food) != ITEM_FOOD) && !access_level(ch, LVL_ADMIN)) {
    send_to_char("You can't eat THAT!\r\n", ch);
    return;
  }
  if (GET_COND(ch, FULL) > 20) {/* Stomach full */
    act("You are too full to eat more!", FALSE, ch, 0, 0, TO_CHAR);
    return;
  }
  if (subcmd == SCMD_EAT) {
    act("You eat $p.", FALSE, ch, food, 0, TO_CHAR);
    act("$n eats $p.", TRUE, ch, food, 0, TO_ROOM);
    if (GET_OBJ_SPEC(food) && GET_OBJ_SPEC(food) == anticoagulant) {
      for (cyber = ch->bioware; cyber; cyber = cyber->next_content)
        if (GET_OBJ_VAL(cyber, 0) == BIO_PLATELETFACTORY) {
          GET_OBJ_VAL(cyber, 5) = 36;
          GET_OBJ_VAL(cyber, 6) = 0;
          break;
        }
    }
  } else {
    act("You nibble a little bit of the $o.", FALSE, ch, food, 0, TO_CHAR);
    act("$n tastes a little bit of $p.", TRUE, ch, food, 0, TO_ROOM);
  }

  amount = (subcmd == SCMD_EAT ? GET_OBJ_VAL(food, 0) : 1);

  gain_condition(ch, FULL, amount);

  if (GET_COND(ch, FULL) > 20)
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
      send_to_char(NOOBJECT, ch);
      return;
    }
    if (GET_OBJ_TYPE(from_obj) != ITEM_DRINKCON) {
      act("You can't pour from that!", FALSE, ch, 0, 0, TO_CHAR);
      return;
    }
  }
  if (subcmd == SCMD_FILL) {
    if (!*arg1) {               /* no arguments */
      send_to_char("What do you want to fill?  And what are you filling it from?\r\n", ch);
      return;
    }
    if (!(to_obj = get_obj_in_list_vis(ch, arg1, ch->carrying))) {
      send_to_char(NOOBJECT, ch);
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
    if (!(from_obj = get_obj_in_list_vis(ch, arg2, world[ch->in_room].contents))) {
      sprintf(buf, "There doesn't seem to be %s %s here.\r\n", AN(arg2), arg2);
      send_to_char(buf, ch);
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
      send_to_char(NOOBJECT, ch);
      return;
    }
    if ((GET_OBJ_TYPE(to_obj) != ITEM_DRINKCON) &&
        (GET_OBJ_TYPE(to_obj) != ITEM_FOUNTAIN)) {
      act("You can't pour anything into that.", FALSE, ch, 0, 0, TO_CHAR);
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
    sprintf(buf, "You pour the %s into the %s.",
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
                                "You put $p around your thigh"},

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
  if (GET_OBJ_TYPE(one) != ITEM_WEAPON || GET_OBJ_TYPE(one) != ITEM_WEAPON)
    return TRUE;
  if ((IS_GUN(GET_OBJ_VAL(one, 3)) && !IS_GUN(GET_OBJ_VAL(two, 3))) ||
      (IS_GUN(GET_OBJ_VAL(two, 3)) && !IS_GUN(GET_OBJ_VAL(one, 3))))
    return FALSE;
  else if (IS_OBJ_STAT(one, ITEM_TWOHANDS) || IS_OBJ_STAT(two, ITEM_TWOHANDS))
    return FALSE;

  return TRUE;
}

void perform_wear(struct char_data * ch, struct obj_data * obj, int where)
{
  struct obj_data *wielded = GET_EQ(ch, WEAR_WIELD);
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
                              "You're already wearing something on your feet.\r\n"
                            };

  /* first, make sure that the wear position is valid. */
  if (!CAN_WEAR(obj, wear_bitvectors[where]))
  {
    act("You can't wear $p there.", FALSE, ch, obj, 0, TO_CHAR);
    return;
  }
  switch (GET_RACE(ch))
  {
  case RACE_WAKYAMBI:
  case RACE_DRYAD:
  case RACE_ELF:
  case RACE_NIGHTONE:
    if (IS_OBJ_STAT(obj, ITEM_NOELF)) {
      send_to_char(ch, "You can't wear that.\r\n");
      return;
    }
    break;
  case RACE_DWARF:
  case RACE_MENEHUNE:
  case RACE_GNOME:
  case RACE_KOBOROKURU:
    if (IS_OBJ_STAT(obj, ITEM_NODWARF)) {
      send_to_char(ch, "You can't wear that.\r\n");
      return;
    }
    break;
  case RACE_TROLL:
  case RACE_MINOTAUR:
  case RACE_GIANT:
  case RACE_FOMORI:
  case RACE_CYCLOPS:
    if (IS_OBJ_STAT(obj, ITEM_NOTROLL)) {
      send_to_char(ch, "You can't wear that.\r\n");
      return;
    }
    break;
  case RACE_HUMAN:
    if (IS_OBJ_STAT(obj, ITEM_NOHUMAN)) {
      send_to_char(ch, "You can't wear that.\r\n");
      return;
    }
    break;
  case RACE_ONI:
  case RACE_ORK:
  case RACE_OGRE:
  case RACE_HOBGOBLIN:
    if (IS_OBJ_STAT(obj, ITEM_NOORK)) {
      send_to_char(ch, "You can't wear that.\r\n");
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


  if ((where == WEAR_WIELD || where == WEAR_HOLD) && IS_OBJ_STAT(obj, ITEM_TWOHANDS) &&
      (GET_EQ(ch, WEAR_SHIELD) || GET_EQ(ch, WEAR_HOLD) || GET_EQ(ch, WEAR_WIELD)))
  {
    act("$p requires two free hands.", FALSE, ch, obj, 0, TO_CHAR);
    return;
  }

  if (where == WEAR_WIELD)
  {
    if (!wielded) {
      if (!can_wield_both(ch, obj, GET_EQ(ch, WEAR_HOLD))) {
        act("That wouldn't work very well.", FALSE, ch, 0, 0, TO_CHAR);
        return;
      }
    } else {
      /* if attempting to wield a second weapon... */
      if (GET_EQ(ch, WEAR_HOLD) || GET_EQ(ch, WEAR_SHIELD)) {
        send_to_char("Your hands are already full!\r\n", ch);
        return;
      }
      where = WEAR_HOLD;
      if (!can_wield_both(ch, wielded, obj)) {
        act("That wouldn't work very well.", FALSE, ch, 0, 0, TO_CHAR);
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
    act("$p requires at least one free hand.", FALSE, ch, obj, 0, TO_CHAR );
    return;
  }

  if (GET_EQ(ch, WEAR_WIELD) && IS_OBJ_STAT(GET_EQ(ch, WEAR_WIELD), ITEM_TWOHANDS) &&
      (where == WEAR_HOLD || where == WEAR_SHIELD))
  {
    act("$p requires two free hands.", FALSE, ch, GET_EQ(ch, WEAR_WIELD), 0, TO_CHAR);
    return;
  } else if (GET_EQ(ch, WEAR_HOLD) && IS_OBJ_STAT(GET_EQ(ch, WEAR_HOLD), ITEM_TWOHANDS) &&
             (where == WEAR_WIELD || where == WEAR_SHIELD))
  {
    act("$p requires two free hands.", FALSE, ch, GET_EQ(ch, WEAR_HOLD), 0, TO_CHAR);
    return;
  }

  if (IS_OBJ_STAT(obj, ITEM_FORMFIT)) {
    if (GET_EQ(ch, WEAR_BODY) && GET_LEGAL_NUM(GET_EQ(ch, WEAR_BODY))) {
      send_to_char(ch, "You can't wear that with %s.\r\n", GET_OBJ_NAME(GET_EQ(ch, WEAR_BODY)));
      return;
    }
    if (GET_EQ(ch, WEAR_ABOUT) && GET_LEGAL_NUM(GET_EQ(ch, WEAR_ABOUT))) {
      send_to_char(ch, "You can't wear that with %s.\r\n", GET_OBJ_NAME(GET_EQ(ch, WEAR_ABOUT)));
      return;
    }
  }
  if ((where == WEAR_ABOUT || where == WEAR_BODY) && GET_LEGAL_NUM(obj) && GET_EQ(ch, WEAR_UNDER) && IS_OBJ_STAT(GET_EQ(ch, WEAR_UNDER), ITEM_FORMFIT)) {
    send_to_char(ch, "You can't wear that with %s.\r\n", GET_OBJ_NAME(GET_EQ(ch, WEAR_UNDER)));
    return;
  }
  wear_message(ch, obj, where);
  if (obj->in_obj)
    obj_from_obj(obj);
  else
    obj_from_char(obj);
  equip_char(ch, obj, where);
  int total = 0;
  if (GET_TOTALBAL(ch) > GET_QUI(ch))
    total += GET_TOTALBAL(ch) - GET_QUI(ch);
  else if (GET_TOTALIMP(ch) > GET_QUI(ch))
    total += GET_TOTALIMP(ch) - GET_QUI(ch);
  if (total >= GET_QUI(ch))
    send_to_char("You are wearing so much armour that you can't move!\r\n", ch);
  else if (total >= GET_QUI(ch) * 3/4)
    send_to_char("Your movement is severely restricted by your armour.\r\n", ch);
  else if (total >= GET_QUI(ch) / 2)
    send_to_char("Your movement is restricted by your armour.\r\n", ch);
  else if (total)
    send_to_char("Your movement is mildly restricted by your armour.\r\n", ch);
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
      "under",
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
      "legs",
      "thigh",
      "!RESERVED!",
      "ankle",
      "!RESERVED!",
      "sock",
      "feet",
      "\n"
    };


  if (!arg || !*arg)
  {
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
  } else
  {
    if ((where = search_block(arg, keywords, FALSE)) < 0) {
      sprintf(buf, "'%s'?  What part of your body is THAT?\r\n", arg);
      send_to_char(buf, ch);
    }
  }

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
    send_to_char("Now that would be a good trick!\r\n", ch);
    return;
  }

  if (!*arg1) {
    send_to_char("Wear what?\r\n", ch);
    return;
  }
  dotmode = find_all_dots(arg1);

  if (*arg2 && (dotmode != FIND_INDIV)) {
    send_to_char("You can't specify the same body location for more than one item!\r\n", ch);
    return;
  }
  if (dotmode == FIND_ALL) {
    for (obj = ch->carrying; obj; obj = next_obj) {
      next_obj = obj->next_content;
      if (CAN_SEE_OBJ(ch, obj) && (where = find_eq_pos(ch, obj, 0)) >= 0) {
        items_worn++;
        perform_wear(ch, obj, where);
      }
    }
    if (!items_worn)
      send_to_char("You don't seem to have anything wearable.\r\n", ch);
  } else if (dotmode == FIND_ALLDOT) {
    if (!*arg1) {
      send_to_char("Wear all of what?\r\n", ch);
      return;
    }
    if (!(obj = get_obj_in_list_vis(ch, arg1, ch->carrying))) {
      sprintf(buf, "You don't seem to have any %ss.\r\n", arg1);
      send_to_char(buf, ch);
    } else
      while (obj) {
        next_obj = get_obj_in_list_vis(ch, arg1, obj->next_content);
        if ((where = find_eq_pos(ch, obj, 0)) >= 0)
          perform_wear(ch, obj, where);
        else
          act("You can't wear $p.", FALSE, ch, obj, 0, TO_CHAR);
        obj = next_obj;
      }
  } else {
    if (!(obj = get_obj_in_list_vis(ch, arg1, ch->carrying))) {
      sprintf(buf, "You don't seem to have %s %s.\r\n", AN(arg1), arg1);
      send_to_char(buf, ch);
    } else {
      if ((where = find_eq_pos(ch, obj, arg2)) >= 0)
        perform_wear(ch, obj, where);
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
    sprintf(buf, "You don't seem to have %s %s.\r\n", AN(arg), arg);
    send_to_char(buf, ch);
  } else {
    if (!CAN_WEAR(obj, ITEM_WEAR_WIELD))
      send_to_char("You can't wield that.\r\n", ch);
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
        send_to_char(ch, "It's too heavy for you to wield effectively.\r\n");
      else
        perform_wear(ch, obj, WEAR_WIELD);
    } else
      perform_wear(ch, obj, WEAR_WIELD);
  }
}

ACMD(do_grab)
{
  struct obj_data *obj;

  one_argument(argument, arg);

  if (!*arg)
    send_to_char("Hold what?\r\n", ch);
  else if (!(obj = get_obj_in_list_vis(ch, arg, ch->carrying))) {
    sprintf(buf, "You don't seem to have %s %s.\r\n", AN(arg), arg);
    send_to_char(buf, ch);
  } else {
    if (GET_OBJ_TYPE(obj) == ITEM_LIGHT)
      perform_wear(ch, obj, WEAR_LIGHT);
    else
      perform_wear(ch, obj, WEAR_HOLD);
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
  if (IS_CARRYING_N(ch) >= CAN_CARRY_N(ch))
    act("$p: you can't carry that many items!", FALSE, ch, obj, 0, TO_CHAR);
  else
  {
    obj_to_char(unequip_char(ch, pos, TRUE), ch);
    act("You stop using $p.", FALSE, ch, obj, 0, TO_CHAR);
    act("$n stops using $p.", TRUE, ch, obj, 0, TO_ROOM);

    if (GET_OBJ_TYPE(obj) == ITEM_HOLSTER)
      GET_OBJ_VAL(obj, 3) = 0;
    /* add damage back from stim patches */
    /* it doesn't do anything to keep track, */
    /* so I'm just makeing it a mod mental damage to it */
    if ( GET_OBJ_TYPE(obj) == ITEM_PATCH && GET_OBJ_VAL(obj, 0) == 1 ) {
      GET_MENTAL(ch) = GET_OBJ_VAL(obj,5);
      GET_OBJ_VAL(obj,5) = 0;
    }
  }
}

ACMD(do_remove)
{
  struct obj_data *obj;
  int i, dotmode, found;

  one_argument(argument, arg);

  if (AFF_FLAGGED(ch, AFF_PILOT)) {
    send_to_char("Now that would be a good trick!\r\n", ch);
    return;
  }

  if (!*arg) {
    send_to_char("Remove what?\r\n", ch);
    return;
  }

  dotmode = find_all_dots(arg);

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
        sprintf(buf, "You don't seem to be using any %ss.\r\n", arg);
        send_to_char(buf, ch);
      }
    }
  } else {
    if (!(obj = get_object_in_equip_vis(ch, arg, ch->equipment, &i))) {
      sprintf(buf, "You don't seem to be using %s %s.\r\n", AN(arg), arg);
      send_to_char(buf, ch);
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

  if (GET_TRADITION(ch) == TRAD_ADEPT && !str_str(argument, "Pain editor")) {
    char name[120], tokens[MAX_STRING_LENGTH], *s;
    extern int ability_cost(int abil, int level);
    int x;
    strcpy(tokens, argument);
    if (strtok(tokens, "\"") && (s = strtok(NULL, "\""))) {
      strcpy(name, s);
      if ((s = strtok(NULL, "\0"))) {
        skip_spaces(&s);
        strcpy(buf1, s);
      } else
        *buf1 = '\0';
      one_argument(argument, buf);
      x = atoi(buf);
    } else {
      half_chop(argument, buf, buf1);
      if (!(x = atoi(buf))) {
        strcpy(name, buf);
      } else {
        half_chop(buf1, buf2, buf1);
        strcpy(name, buf2);
      }
    }

    for (i = 0; i < ADEPT_NUMPOWER; i++)
      if (GET_POWER_TOTAL(ch, i) && is_abbrev(name, adept_powers[i]))
        break;
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
      if (total + GET_POWER_POINTS(ch) > ((int)(GET_REAL_MAG(ch) / 100) * 100))
        send_to_char("You have too many powers activated already.\r\n", ch);
      else {
        GET_POWER_ACT(ch, i) = x;
        GET_POWER_POINTS(ch) += total;
        send_to_char(ch, "You activate %s.\r\n", adept_powers[i]);
        affect_total(ch);
      }
      return;
    }
  }

  any_one_arg(argument, arg);

  if (is_abbrev(arg, "pain editor")) {
    for (obj = ch->bioware; obj; obj = obj->next_content)
      if (GET_OBJ_VAL(obj, 0) == BIO_PAINEDITOR) {
        if (GET_OBJ_VAL(obj, 3))
          send_to_char("You have already activated your Pain Editor.\r\n", ch);
        else {
          GET_OBJ_VAL(obj, 3) = 1;
          send_to_char("You lose all tactile perception.\r\n", ch);
        }
        return;
      }
  }
  if (is_abbrev(arg, "voice modulator")) {
    for (obj = ch->cyberware; obj; obj = obj->next_content)
      if (GET_OBJ_VAL(obj, 0) == CYB_VOICEMOD) {
        if (GET_OBJ_VAL(obj, 3))
          send_to_char("You have already activated your Voice Modulator.\r\n", ch);
        else {
          GET_OBJ_VAL(obj, 3) = 1;
          send_to_char("You begin to speak like Stephen Hawking.\r\n", ch);
        }
        return;
      }   
  }

  if (!(obj = get_obj_in_list_vis(ch, arg, ch->carrying)))
    if (!(obj = get_object_in_equip_vis(ch, arg, ch->equipment, &i))) {
      send_to_char(ch, "You don't have %s %s.\r\n", AN(arg), arg);
      return;
    }
  if (GET_OBJ_TYPE(obj) == ITEM_FOCUS && GET_IDNUM(ch) == GET_OBJ_VAL(obj, 2)) {
    if (obj->worn_on == NOWHERE) {
      send_to_char("You have to be wearing or holding a focus to activate it.\r\n", ch);
      return;
    }
    if (GET_OBJ_VAL(obj, 9) > 0) {
      send_to_char("You haven't finished bonding that yet.\r\n", ch);
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
  } else if (GET_OBJ_TYPE(obj) != ITEM_MONEY || !GET_OBJ_VAL(obj, 1)) {
    send_to_char("You can't activate that.\r\n", ch);
    return;
  } else if (GET_OBJ_VAL(obj, 4) != 0) {
    send_to_char("But that has already been activated!\r\n", ch);
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
    act("$p scans your retina and the display flashes green.\r\n", FALSE, ch, obj, 0, TO_CHAR);
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
      send_to_char(ch, "You don't have %s %s.", AN(arg), arg);
      return;
    }

  if (GET_OBJ_TYPE(obj) != ITEM_MONEY || !GET_OBJ_VAL(obj, 1) || GET_OBJ_VAL(obj, 2) != 1) {
    send_to_char("You can't type a code into that!\r\n", ch);
    return;
  }

  i = atoi(buf);

  if (i < 100000 || i > 999999)
    send_to_char("Code must be 6-digits!\r\n", ch);
  else if (i != GET_OBJ_VAL(obj, 5))
    send_to_char("The display flashes red and beeps annoyingly.\r\n", ch);
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
    send_to_char("And how do you plan on doing that?\r\n", ch);
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
    sprintf(rbuf,"Crack: Skill %d, Rating %d, Modify Target %d.\n",
            skill, rating, modify_target(ch));
    act(rbuf, FALSE, ch, NULL, NULL, TO_ROLLS);
  }

  if (success_test(skill, rating + modify_target(ch)) < 1) {
    act("$p sounds a series of beeps, and flashes red.", FALSE, ch, obj, 0, TO_CHAR);
    GET_OBJ_ATTEMPT(obj)++;
  } else {
    act("$p hums silently, and its display flashes green.", FALSE, ch, obj, 0, TO_CHAR);
    GET_OBJ_TIMER(obj) = GET_OBJ_VAL(obj, 3) = GET_OBJ_VAL(obj, 4) = GET_OBJ_VAL(obj, 5) = 0;
  }
}

int draw_weapon(struct char_data *ch)
{
  struct obj_data *hols, *obj;
  int i = 0;

  if (!GET_EQ(ch, WEAR_WIELD) || !GET_EQ(ch, WEAR_HOLD))
  {
    for (int x = 0; x < NUM_WEARS; x++)
      if (GET_EQ(ch, x)) {
        if (GET_OBJ_TYPE(GET_EQ(ch, x)) == ITEM_HOLSTER && GET_OBJ_VAL(GET_EQ(ch, x), 3)) {
          hols = GET_EQ(ch, x)->contains;
          if (!hols) {
            GET_OBJ_VAL(GET_EQ(ch, x), 3) = 0;
            return 0;
          }
          if ((GET_EQ(ch, WEAR_WIELD) && GET_EQ(ch, WEAR_HOLD)) || ((GET_EQ(ch, WEAR_WIELD) || GET_EQ(ch, WEAR_HOLD)) && IS_OBJ_STAT(hols, ITEM_TWOHANDS)))
            continue;
          if (GET_OBJ_VAL(hols, 4) >= SKILL_MACHINE_GUNS && GET_OBJ_VAL(hols, 4) <= SKILL_ASSAULT_CANNON)
            continue;
          int where = 0;
          if (!GET_EQ(ch, WEAR_WIELD) && can_wield_both(ch, GET_EQ(ch, WEAR_HOLD), hols))
            where = WEAR_WIELD;
          else if (!GET_EQ(ch, WEAR_HOLD) && can_wield_both(ch, GET_EQ(ch, WEAR_WIELD), hols))
            where = WEAR_HOLD;
          if (where) {
            obj_from_obj(hols);
            equip_char(ch, hols, where);
            act("You draw $p from $P.", FALSE, ch, hols, GET_EQ(ch, x), TO_CHAR);
            act("$n draws $p from $P.", TRUE, ch, hols, GET_EQ(ch, x), TO_ROOM);
            i++;
          }
        } else if (GET_OBJ_TYPE(GET_EQ(ch, x)) == ITEM_WORN)
          for (obj = GET_EQ(ch, x)->contains; obj; obj = obj->next_content)
            if (GET_OBJ_TYPE(obj) == ITEM_HOLSTER && GET_OBJ_VAL(obj, 3)) {
              hols = obj->contains;
              if (!hols) {
                GET_OBJ_VAL(GET_EQ(ch, x), 3) = 0;
                return 0;
              }
              if ((GET_EQ(ch, WEAR_WIELD) && GET_EQ(ch, WEAR_HOLD)) || ((GET_EQ(ch, WEAR_WIELD) || GET_EQ(ch, WEAR_HOLD)) && IS_OBJ_STAT(hols, ITEM_TWOHANDS)))
                continue;
              if (GET_OBJ_VAL(hols, 4) >= SKILL_MACHINE_GUNS && GET_OBJ_VAL(hols, 4) <= SKILL_ASSAULT_CANNON)
                continue;
              int where = 0;
              if (!GET_EQ(ch, WEAR_WIELD) &&  can_wield_both(ch, GET_EQ(ch, WEAR_HOLD), hols))
                where = WEAR_WIELD;
              else if (!GET_EQ(ch, WEAR_HOLD) && can_wield_both(ch, GET_EQ(ch, WEAR_WIELD), hols))
                where = WEAR_HOLD;
              if (where) {
                obj_from_obj(hols);
                equip_char(ch, hols, where);
                act("You draw $p from $P.", FALSE, ch, hols, GET_EQ(ch, x), TO_CHAR);
                act("$n draws $p from $P.", TRUE, ch, hols, GET_EQ(ch, x), TO_ROOM);
                i++;
              }
            }
       }
  }
  affect_total(ch);
  return i;
}

ACMD(do_holster)
{ // Holster <gun> <holster>
  struct obj_data *obj = NULL, *cont = NULL;
  struct char_data *tmp_char;
  int dontfit = 0;

  if (IS_ASTRAL(ch)) {
    send_to_char("You can't!\r\n", ch);
    return;
  }
  two_arguments(argument, buf, buf1);
  if (!generic_find(buf, FIND_OBJ_EQUIP | FIND_OBJ_INV, ch, &tmp_char, &obj)) {
    send_to_char("You need to holster a weapon into a holster.\r\n", ch);
    return;
  }
  if (!generic_find(buf1, FIND_OBJ_EQUIP | FIND_OBJ_INV, ch, &tmp_char, &cont)) {
    for (int x = 0; x < NUM_WEARS; x++) {
      if (GET_EQ(ch, x) && GET_OBJ_TYPE(GET_EQ(ch, x)) == ITEM_WORN && GET_EQ(ch, x)->contains) {
        for (struct obj_data *temp = GET_EQ(ch, x)->contains; temp; temp = temp->next_content)
          if (GET_OBJ_TYPE(temp) == ITEM_HOLSTER && !temp->contains) {
            cont = temp;
            break;
          }
      } else if (GET_EQ(ch, x) && GET_OBJ_TYPE(GET_EQ(ch, x)) == ITEM_HOLSTER && !GET_EQ(ch, x)->contains) {
          cont = GET_EQ(ch, x);
          break;
      }
    }
  }
  if (!cont) {
    send_to_char("You need to holster a weapon into a holster.\r\n", ch);
    return;
  }
  if (GET_OBJ_TYPE(obj) != ITEM_WEAPON || GET_OBJ_TYPE(cont) != ITEM_HOLSTER) {
    send_to_char("You need to holster a weapon into a holster.\r\n", ch);
    return;
  }
  if (cont->contains) {
    send_to_char("There is already something in there.\r\n", ch);
    return;
  }

  switch (GET_OBJ_VAL(cont, 0)) {
  case 0:
    if (!IS_GUN(GET_OBJ_VAL(obj, 3)))
      dontfit++;
    else if (!(GET_OBJ_VAL(obj, 4) == SKILL_PISTOLS || GET_OBJ_VAL(obj, 4) == SKILL_SMG))
      dontfit++;
    break;
  case 1:
    if (IS_GUN(GET_OBJ_VAL(obj, 3)))
      dontfit++;
    break;
  case 2:
    if (!IS_GUN(GET_OBJ_VAL(obj, 3)))
      dontfit++;
    else if (GET_OBJ_VAL(obj, 4) == SKILL_PISTOLS || GET_OBJ_VAL(obj, 4) == SKILL_SMG)
      dontfit++;
    break;
  }
  if (dontfit) {
    send_to_char("It doesn't seem to fit.\r\n", ch);
    return;
  }
  if (obj->worn_by)
    obj = unequip_char(ch, obj->worn_on, TRUE);
  else
    obj_from_char(obj);
  obj_to_obj(obj, cont);
  sprintf(buf2, "You slip %s into %s.\r\n", GET_OBJ_NAME(obj), GET_OBJ_NAME(cont));
  send_to_char(buf2, ch);
  act("$n slips $p into $P.", FALSE, ch, obj, cont, TO_ROOM);
  return;
}

ACMD(do_ready)
{
  struct obj_data *obj;
  struct char_data *tmp_char;
  int num = 0;

  one_argument(argument, buf);

  if (!*buf) {
    send_to_char(ch, "You have to ready something.\r\n");
    return;
  }

  if (!(generic_find(buf, FIND_OBJ_EQUIP, ch, &tmp_char, &obj))) {
    sprintf(buf, "You don't seem to be using %s %s.\r\n", AN(argument), argument);
    send_to_char(buf, ch);
    return;
  }
  if (GET_OBJ_TYPE(obj) != ITEM_HOLSTER) {
    send_to_char(ch, "That is not a weapons holster.\r\n");
    return;
  }
  if (!obj->contains) {
    send_to_char(ch, "There is nothing in it.\r\n");
    return;
  }
  if (GET_OBJ_VAL(obj, 3) > 0) {
    act("You unready $p.", FALSE, ch, obj, NULL, TO_CHAR);
    GET_OBJ_VAL(obj, 3) = 0;
    return;
  } else {
    for (int i = 0; i < NUM_WEARS; i++)
      if (GET_EQ(ch, i)) {
        if (GET_OBJ_TYPE(GET_EQ(ch, i)) == ITEM_HOLSTER && GET_OBJ_VAL(GET_EQ(ch, i), 3) > 0)
          num++;
        else if (GET_OBJ_TYPE(GET_EQ(ch, i)) == ITEM_WORN && GET_EQ(ch, i)->contains)
          for (struct obj_data *cont = GET_EQ(ch, i)->contains; cont; cont = cont->next_content)
            if (GET_OBJ_TYPE(cont) == ITEM_HOLSTER && GET_OBJ_VAL(cont, 3) > 0)
              num++;
      }
    if (num >= 2) {
      send_to_char(ch , "You already have 2 holsters readied.\r\n");
      return;
    }
    act("You ready $p.", FALSE, ch, obj, NULL, TO_CHAR);
    GET_OBJ_VAL(obj, 3) = 1;
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
      send_to_char(ch, "You have nothing to draw.\r\n");
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
    send_to_char("Breaking that now would be a waste.\r\n", ch);
  else {
    extern void do_drug_take(struct char_data *ch, struct obj_data *obj);
    send_to_char("You bite down hard on the tooth compartment, breaking it open.\r\n", ch);
    obj_from_cyberware(obj);
    GET_ESSHOLE(ch) += GET_OBJ_VAL(obj, 4);
    do_drug_take(ch, contents);
    extract_obj(obj);
  }
}
