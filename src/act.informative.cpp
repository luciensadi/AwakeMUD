/*************************************************************************
 *  File: act.informative.c                             Part of CircleMUD *
 *  Usage: Player-level commands of an informative nature                  *
 *                                                                         *
 *  All rights reserved.  See license.doc for complete information.        *
 *                                                                         *
 *  Copyright (C) 1993, 94 by the Trustees of the Johns Hopkins University *
 *  CircleMUD is based on DikuMUD, Copyright (C) 1990, 1991.               *
 ************************************************************************ */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <time.h>
#include <errno.h>
#include <iostream>
#include <fstream>

using namespace std;

#if !defined(WIN32) || defined(__CYGWIN__)
#include <sys/time.h>
#endif

#include "structs.h"
#include "utils.h"
#include "comm.h"
#include "interpreter.h"
#include "handler.h"
#include "db.h"
#include "dblist.h"
#include "screen.h"
#include "list.h"
#include "awake.h"
#include "constants.h"
#include "quest.h"
#include "transport.h"
#include "newdb.h"

const char *CCHAR;

extern bool DISPLAY_HELPFUL_STRINGS_FOR_MOB_FUNCS;

/* extern variables */
extern class objList ObjList;
extern class helpList Help;
extern class helpList WizHelp;

extern char *short_object(int virt, int where);
extern const char *dist_name[];

extern int same_obj(struct obj_data * obj1, struct obj_data * obj2);
extern int find_sight(struct char_data *ch);
extern int belongs_to(struct char_data *ch, struct obj_data *obj);

extern int get_weapon_damage_type(struct obj_data* weapon);

extern SPECIAL(trainer);
extern SPECIAL(teacher);
extern SPECIAL(metamagic_teacher);
extern SPECIAL(adept_trainer);
extern SPECIAL(spell_trainer);
extern SPECIAL(johnson);
extern SPECIAL(shop_keeper);
extern SPECIAL(landlord_spec);

extern bool trainable_attribute_is_maximized(struct char_data *ch, int attribute);

extern teach_t teachers[];

extern struct elevator_data *elevator;
extern int num_elevators;

/* blood stuff */

const char* blood_messages[] = {
  "If you see this, alert an immort.\r\n", /* 0 */
  "^rThere is a little blood here.\r\n",
  "^rYou are standing in a pool of blood.\r\n",
  "^rBlood is flowing here. Gross.\r\n",
  "^rThere's a lot of blood here...you feel sick.\r\n",
  "^rYou've seen less blood at a GWAR concert.\r\n", /* 5 */
  "^rCripes, there's blood EVERYWHERE. You can taste it in the air.\r\n",
  "^rThe walls are practically painted red with blood.\r\n",
  "^rBlood drips from the walls and ceiling, and covers the floor an inch deep.\r\n",
  "^rThere is gore, blood, guts and parts everywhere. The stench is horrible.\r\n",
  "^rThe gore is indescribable, and you feel numb and light headed.\r\n", /* 10 */
  "If you see this, alert an immort that the blood level is too high.\r\n"
};


/* end blood stuff */

char *make_desc(struct char_data *ch, struct char_data *i, char *buf, int act, bool dont_capitalize_a_an)
{
  char buf2[MAX_STRING_LENGTH];
  if (!IS_NPC(i) && ((GET_EQ(i, WEAR_HEAD) && GET_OBJ_VAL(GET_EQ(i, WEAR_HEAD), 7) > 1) ||
                     (GET_EQ(i, WEAR_FACE) && GET_OBJ_VAL(GET_EQ(i, WEAR_FACE), 7) > 1)) && (act == 2 ||
                                                                                             success_test(GET_INT(ch) + GET_POWER(ch, ADEPT_IMPROVED_PERCEPT),
                                                                                                          (GET_EQ(i, WEAR_HEAD) ? GET_OBJ_VAL(GET_EQ(i, WEAR_HEAD), 7) : 0) +
                                                                                                          (GET_EQ(i, WEAR_FACE) ? GET_OBJ_VAL(GET_EQ(i, WEAR_FACE), 7) : 0)) < 1))
  {
    int conceal = (GET_EQ(i, WEAR_ABOUT) ? GET_OBJ_VAL(GET_EQ(i, WEAR_ABOUT), 7) : 0) +
    (GET_EQ(i, WEAR_BODY) ? GET_OBJ_VAL(GET_EQ(i, WEAR_BODY), 7) : 0) +
    (GET_EQ(i, WEAR_UNDER) ? GET_OBJ_VAL(GET_EQ(i, WEAR_UNDER), 7) : 0);
    conceal = act == 2 ? 4 : success_test(GET_INT(ch) + GET_POWER(ch, ADEPT_IMPROVED_PERCEPT), conceal);
    sprintf(buf, "%s", dont_capitalize_a_an ? "a" : "A");
    if (conceal > 0) {
      if (GET_HEIGHT(i) < 130)
        strcat(buf, " tiny");
      else if (GET_HEIGHT(i) < 160)
        strcat(buf, " small");
      else if (GET_HEIGHT(i) < 190)
        strcat(buf, "n average");
      else if (GET_HEIGHT(i) < 220)
        strcat(buf, " large");
      else
        strcat(buf, " huge");
    }
    if (conceal > 2)
      sprintf(buf + strlen(buf), " %s", genders[(int)GET_SEX(i)]);
    if (conceal > 3)
      sprintf(buf + strlen(buf), " %s", pc_race_types[(int)GET_RACE(i)]);
    else
      strcat(buf, " person");
    if (GET_EQ(i, WEAR_ABOUT))
      sprintf(buf + strlen(buf), " wearing %s", decapitalize_a_an(GET_OBJ_NAME(GET_EQ(i, WEAR_ABOUT))));
    else if (GET_EQ(i, WEAR_BODY))
      sprintf(buf + strlen(buf), " wearing %s", decapitalize_a_an(GET_OBJ_NAME(GET_EQ(i, WEAR_BODY))));
    else if (GET_EQ(i, WEAR_UNDER))
      sprintf(buf + strlen(buf), " wearing %s", decapitalize_a_an(GET_OBJ_NAME(GET_EQ(i, WEAR_UNDER))));
  } else
  {
    struct remem *mem;
    if (!act) {
      strcpy(buf, dont_capitalize_a_an ? decapitalize_a_an(CAP(GET_NAME(i))) : CAP(GET_NAME(i)));
      if (IS_SENATOR(ch) && !IS_NPC(i))
        sprintf(ENDOF(buf), " (%s)", CAP(GET_CHAR_NAME(i)));
      else if ((mem = found_mem(GET_MEMORY(ch), i)))
        sprintf(ENDOF(buf), " (%s)", CAP(mem->mem));
    } else if ((mem = found_mem(GET_MEMORY(ch), i)) && act != 2)
      strcpy(buf, CAP(mem->mem));
    else
      strcpy(buf, dont_capitalize_a_an ? decapitalize_a_an(CAP(GET_NAME(i))) : CAP(GET_NAME(i)));
  }
  if (GET_SUSTAINED(i) && (IS_ASTRAL(ch) || IS_DUAL(ch)))
  {
    for (struct sustain_data *sust = GET_SUSTAINED(i); sust; sust = sust->next)
      if (!sust->caster) {
        strcat(buf, ", surrounded by a spell aura");
        break;
      }
  }
  
  if (!IS_NPC(i) && PRF_FLAGGED(ch, PRF_LONGWEAPON) && GET_EQ(i, WEAR_WIELD))
    sprintf(ENDOF(buf), ", wielding %s", decapitalize_a_an(GET_OBJ_NAME(GET_EQ(i, WEAR_WIELD))));
  
  if (AFF_FLAGGED(i, AFF_MANIFEST) && !(IS_ASTRAL(ch) || IS_DUAL(ch)))
  {
    sprintf(buf2, "The ghostly image of %s", buf);
    strcpy(buf, buf2);
  }
  return buf;
}

void get_obj_condition(struct char_data *ch, struct obj_data *obj)
{
  if (GET_OBJ_EXTRA(obj).IsSet(ITEM_CORPSE))
  {
    send_to_char("Examining it reveals that it really IS dead.\r\n", ch);
    return;
  }
  
  int condition = GET_OBJ_CONDITION(obj) * 100 / MAX(1, GET_OBJ_BARRIER(obj));
  sprintf(buf2, "%s is ", CAP(GET_OBJ_NAME(obj)));
  if (condition >= 100)
    strcat(buf2, "in excellent condition.");
  else if (condition >= 90)
    strcat(buf2, "barely damaged.");
  else if (condition >= 80)
    strcat(buf2, "lightly damaged.");
  else if (condition >= 60)
    strcat(buf2, "moderately damaged.");
  else if (condition >= 30)
    strcat(buf2, "seriously damaged.");
  else if (condition >= 10)
    strcat(buf2, "extremely damaged.");
  else
    strcat(buf2, "almost completely destroyed.");
  strcat(buf2, "\r\n");
  send_to_char(ch, buf2);
}

void show_obj_to_char(struct obj_data * object, struct char_data * ch, int mode)
{
  *buf = '\0';
  if ((mode == 0) && object->text.room_desc)
  {
    strcpy(buf, CCHAR ? CCHAR : "");
    if (object->graffiti)
      strcat(buf, object->graffiti);
    else strcat(buf, object->text.room_desc);
  } else if (GET_OBJ_NAME(object) && mode == 1)
  {
    strcpy(buf, GET_OBJ_NAME(object));
    if (GET_OBJ_TYPE(object) == ITEM_DESIGN)
      sprintf(ENDOF(buf), " (Plan)");
    if (GET_OBJ_VNUM(object) == 108 && !GET_OBJ_TIMER(object))
      sprintf(ENDOF(buf), " (Uncooked)");
    if (GET_OBJ_TYPE(object) == ITEM_FOCUS && GET_OBJ_VAL(object, 9) == GET_IDNUM(ch))
      sprintf(ENDOF(buf), " ^Y(Geas)^n");
  } else if (GET_OBJ_NAME(object) && ((mode == 2) || (mode == 3) || (mode == 4) || (mode == 7)))
    strcpy(buf, GET_OBJ_NAME(object));
  else if (mode == 5)
  {
    if (GET_OBJ_DESC(object))
      strcpy(buf, GET_OBJ_DESC(object));
    else
      strcpy(buf, "You see nothing special..");
  } else if (mode == 8)
    sprintf(buf, "\t\t\t\t%s", GET_OBJ_NAME(object));
  if (mode == 7 || mode == 8) {
    if (GET_OBJ_TYPE(object) == ITEM_HOLSTER)
    {
      if (object->contains)
        sprintf(ENDOF(buf), " (Holding %s)", GET_OBJ_NAME(object->contains));
      if (GET_OBJ_VAL(object, 3) == 1 && ((object->worn_by && object->worn_by == ch) ||
                                          (object->in_obj && object->in_obj->worn_by && object->in_obj->worn_by == ch)))
        strcat(buf, " ^Y(Ready)");
    } else if (GET_OBJ_TYPE(object) == ITEM_WORN && object->contains && !PRF_FLAGGED(ch, PRF_COMPACT))
      strcat(buf, " carrying:");
    else if (GET_OBJ_TYPE(object) == ITEM_FOCUS) {
      if (GET_OBJ_VAL(object, 4))
        strcat(buf, " ^m(Activated Focus)^n");
      if (GET_OBJ_VAL(object, 9) == GET_IDNUM(ch))
        strcat(buf, " ^Y(Geas)^n");
    }
  }
  if (mode != 3)
  {
    if (IS_OBJ_STAT(object, ITEM_INVISIBLE)) {
      strcat(buf, " ^B(invisible)");
    }
    
    if ((GET_OBJ_TYPE(object) == ITEM_FOCUS || IS_OBJ_STAT(object, ITEM_MAGIC))
        && (IS_ASTRAL(ch) || IS_DUAL(ch))) {
      strcat(buf, " ^Y(magic aura)");
    }
    
    if (IS_OBJ_STAT(object, ITEM_GLOW)) {
      strcat(buf, " ^W(glowing)");
    }
    
    if (IS_OBJ_STAT(object, ITEM_HUM)) {
      strcat(buf, " ^c(humming)");
    }
  }
  strcat(buf, "^N\r\n");
  send_to_char(buf, ch);
  if ((mode == 7 || mode == 8) && !PRF_FLAGGED(ch, PRF_COMPACT))
    if (GET_OBJ_TYPE(object) == ITEM_WORN && object->contains)
    {
      for (struct obj_data *cont = object->contains; cont; cont = cont->next_content)
        show_obj_to_char(cont, ch, 8);
    }
  
}

void show_veh_to_char(struct veh_data * vehicle, struct char_data * ch)
{
  *buf = '\0';
  
  strcpy(buf, CCHAR ? CCHAR : "");
  
  if (vehicle->damage >= 10)
  {
    sprintf(ENDOF(buf), "%s lies here wrecked.", GET_VEH_NAME(vehicle));
  } else
  {
    if (vehicle->type == VEH_BIKE && vehicle->people && CAN_SEE(ch, vehicle->people))
      sprintf(buf, "%s%s sitting on ", buf, CAP(found_mem(GET_MEMORY(ch), vehicle->people) ?
                                                found_mem(GET_MEMORY(ch), vehicle->people)->mem :
                                                GET_NAME(vehicle->people)));
    switch (vehicle->cspeed) {
      case SPEED_OFF:
        if ((vehicle->type == VEH_BIKE && vehicle->people) || vehicle->restring)
          sprintf(ENDOF(buf), "%s waits here", GET_VEH_NAME(vehicle));
        else
          strcat(buf, vehicle->description);
        break;
      case SPEED_IDLE:
        sprintf(ENDOF(buf), "%s idles here", GET_VEH_NAME(vehicle));
        break;
      case SPEED_CRUISING:
        sprintf(ENDOF(buf), "%s cruises through here", GET_VEH_NAME(vehicle));
        break;
      case SPEED_SPEEDING:
        sprintf(ENDOF(buf), "%s speeds past you", GET_VEH_NAME(vehicle));
        break;
      case SPEED_MAX:
        sprintf(ENDOF(buf), "%s zooms by you", GET_VEH_NAME(vehicle));
        break;
    }
    if (vehicle->towing) {
      sprintf(ENDOF(buf), ", towing %s", GET_VEH_NAME(vehicle->towing));
    }
    strcat(buf, ".");
  }
  if (vehicle->owner && GET_IDNUM(ch) == vehicle->owner)
    strcat(buf, " ^Y(Yours)");
  strcat(buf, "^N\r\n");
  send_to_char(buf, ch);
}

void list_veh_to_char(struct veh_data * list, struct char_data * ch)
{
  struct veh_data *i;
  for (i = list; i; i = i->next_veh)
    if (ch->in_veh != i && ch->char_specials.rigging != i)
      show_veh_to_char(i, ch);
}

#define IS_INVIS(o) IS_OBJ_STAT(o, ITEM_INVISIBLE)

void
list_obj_to_char(struct obj_data * list, struct char_data * ch, int mode,
                 bool show, bool corpse)
{
  struct obj_data *i;
  int num = 1;
  bool found;
  
  found = FALSE;
  
  for (i = list; i; i = i->next_content)
  {
    if ((i->in_veh && ch->in_veh) && i->vfront != ch->vfront)
      continue;
    if (ch->in_veh && i->in_room != ch->in_veh->in_room) {
      if (ch->in_veh->cspeed > SPEED_IDLE) {
        if (get_speed(ch->in_veh) >= 200) {
          if (!success_test(GET_INT(ch) + GET_POWER(ch, ADEPT_IMPROVED_PERCEPT), 7))
            continue;
          else if (get_speed(ch->in_veh) < 200 && get_speed(ch->in_veh) >= 120) {
            if (!success_test(GET_INT(ch) + GET_POWER(ch, ADEPT_IMPROVED_PERCEPT), 6))
              continue;
            else if (get_speed(ch->in_veh) < 120 && get_speed(ch->in_veh) >= 60) {
              if (!success_test(GET_INT(ch) + GET_POWER(ch, ADEPT_IMPROVED_PERCEPT), 5))
                continue;
              else
                if (!success_test(GET_INT(ch) + GET_POWER(ch, ADEPT_IMPROVED_PERCEPT), 4))
                  continue;
            }
          }
        }
      }
    }
    
    
    if (ch->char_specials.rigging) {
      if (ch->char_specials.rigging->cspeed > SPEED_IDLE) {
        if (get_speed(ch->char_specials.rigging) >= 240) {
          if (!success_test(GET_INT(ch) + GET_POWER(ch, ADEPT_IMPROVED_PERCEPT), 6))
            continue;
          else if (get_speed(ch->char_specials.rigging) < 240 && get_speed(ch->char_specials.rigging) >= 180) {
            if (!success_test(GET_INT(ch) + GET_POWER(ch, ADEPT_IMPROVED_PERCEPT), 5))
              continue;
            else if (get_speed(ch->char_specials.rigging) < 180 && get_speed(ch->char_specials.rigging) >= 90) {
              if (!success_test(GET_INT(ch) + GET_POWER(ch, ADEPT_IMPROVED_PERCEPT), 4))
                continue;
              else
                if (!success_test(GET_INT(ch) + GET_POWER(ch, ADEPT_IMPROVED_PERCEPT), 3))
                  continue;
            }
          }
        }
      }
    }
    
    while (i->next_content) {
      if (i->item_number != i->next_content->item_number ||
          strcmp(i->text.name, i->next_content->text.name) ||
          IS_INVIS(i) != IS_INVIS(i->next_content) || (i->restring && !i->next_content->restring) ||
          (!i->restring && i->next_content->restring) ||(i->restring && i->next_content->restring && strcmp(i->restring, i->next_content->restring)))
        break;
      if (CAN_SEE_OBJ(ch, i)) {
        num++;
      }
      i = i->next_content;
    }
    
    
    if (CAN_SEE_OBJ(ch, i)) {
      if (corpse && IS_OBJ_STAT(i, ITEM_CORPSE)) {
        if (num > 1) {
          send_to_char(ch, "(%d) ", num);
        }
        show_obj_to_char(i, ch, mode);
      } else
        if (!corpse && !mode && !IS_OBJ_STAT(i, ITEM_CORPSE)) {
          if (num > 1) {
            send_to_char(ch, "(%d) ", num);
          }
          show_obj_to_char(i, ch, mode);
        } else if (mode) {
          if (num > 1) {
            send_to_char(ch, "(%d) ", num);
          }
          show_obj_to_char(i, ch, mode);
        }
      found = TRUE;
      num = 1;
    }
  }
  
  if (!found && show)
    send_to_char(" Nothing.\r\n", ch);
}

void diag_char_to_char(struct char_data * i, struct char_data * ch)
{
  int ment, phys;
  
  if (GET_MAX_PHYSICAL(i) >= 100)
    phys = (100 * GET_PHYSICAL(i)) / MAX(1, GET_MAX_PHYSICAL(i));
  else
    phys = -1;               /* How could MAX_PHYSICAL be < 1?? */
  
  if (GET_MAX_MENTAL(i) >= 100)
    ment = (100 * GET_MENTAL(i)) / MAX(1, GET_MAX_MENTAL(i));
  else
    ment = -1;
  
  make_desc(ch, i, buf, TRUE, FALSE);
  CAP(buf);
  
  if (phys >= 100 || (GET_TRADITION(i) == TRAD_ADEPT && phys >= 0 &&
                      ((100 - phys) / 10) <= GET_POWER(i, ADEPT_PAIN_RESISTANCE)))
    strcat(buf, " is in excellent physical condition");
  else if (phys >= 90)
    strcat(buf, " has a few scratches");
  else if (phys >= 75)
    strcat(buf, " has some small wounds and bruises");
  else if (phys >= 50)
    strcat(buf, " has quite a few wounds");
  else if (phys >= 30)
    strcat(buf, " has some big nasty wounds and scratches");
  else if (phys >= 15)
    strcat(buf, " looks pretty hurt");
  else if (phys >= 0)
    strcat(buf, " is in awful condition");
  else
    strcat(buf, " is bleeding awfully from big wounds");
  
  if (phys <= 0)
    strcat(buf, " and is unconscious.\r\n");
  else if (ment >= 100 || (GET_TRADITION(i) == TRAD_ADEPT && ment >= 0 &&
                           ((100 - ment) / 10) <= (GET_POWER(i, ADEPT_PAIN_RESISTANCE) -
                                                   (int)((GET_MAX_PHYSICAL(i) - GET_PHYSICAL(i)) / 100))))
    strcat(buf, " and is alert.\r\n");
  else if (ment >= 90)
    strcat(buf, " and is barely tired.\r\n");
  else if (ment >= 75)
    strcat(buf, " and is slightly worn out.\r\n");
  else if (ment >= 50)
    strcat(buf, " and is fatigued.\r\n");
  else if (ment >= 30)
    strcat(buf, " and is weary.\r\n");
  else if (ment >= 10)
    strcat(buf, " and is groggy.\r\n");
  else
    strcat(buf, " and is completely unconscious.\r\n");
  
  
  send_to_char(buf, ch);
}

void look_at_char(struct char_data * i, struct char_data * ch)
{
  int j, found, weight;
  float height;
  struct obj_data *tmp_obj;
  
  
  if (((GET_EQ(i, WEAR_HEAD) && GET_OBJ_VAL(GET_EQ(i, WEAR_HEAD), 7) > 1) ||
       (GET_EQ(i, WEAR_FACE) && GET_OBJ_VAL(GET_EQ(i, WEAR_FACE), 7) > 1)) &&
      success_test(GET_INT(ch) + GET_POWER(ch, ADEPT_IMPROVED_PERCEPT),
                   (GET_EQ(i, WEAR_HEAD) ? GET_OBJ_VAL(GET_EQ(i, WEAR_HEAD), 7) : 0) +
                   (GET_EQ(i, WEAR_FACE) ? GET_OBJ_VAL(GET_EQ(i, WEAR_FACE), 7) : 0)) < 1)
  {
    if (GET_EQ(i, WEAR_HEAD))
      send_to_char(ch, GET_EQ(i, WEAR_HEAD)->text.look_desc);
    else if (GET_EQ(i, WEAR_FACE))
      send_to_char(ch, GET_EQ(i, WEAR_FACE)->text.look_desc);
    if (GET_EQ(i, WEAR_ABOUT))
      send_to_char(ch, GET_EQ(i, WEAR_ABOUT)->text.look_desc);
    else if (GET_EQ(i, WEAR_BODY))
      send_to_char(ch, GET_EQ(i, WEAR_BODY)->text.look_desc);
    else if (GET_EQ(i, WEAR_UNDER))
      send_to_char(ch, GET_EQ(i, WEAR_UNDER)->text.look_desc);
  } else
  {
    if (i->player.physical_text.look_desc)
      send_to_char(i->player.physical_text.look_desc, ch);
    else
      act("You see nothing special about $m.", FALSE, i, 0, ch, TO_VICT);
    
    if (i != ch && GET_HEIGHT(i) > 0 && GET_WEIGHT(i) > 0) {
      if ((GET_HEIGHT(i) % 10) < 5)
        height = (float)(GET_HEIGHT(i) - (GET_HEIGHT(i) % 10)) / 100;
      else
        height = (float)(GET_HEIGHT(i) - (GET_HEIGHT(i) % 10) + 10) / 100;
      if ((GET_WEIGHT(i) % 10) < 5)
        weight = GET_WEIGHT(i) - (GET_WEIGHT(i) % 10);
      else
        weight = GET_WEIGHT(i) - (GET_WEIGHT(i) % 10) + 10;
      
      if (weight > 4) {
        sprintf(buf, "$e looks to be about %0.1f meters tall and "
                "appears to weigh about %d kg.", height, weight);
        act(buf, FALSE, i, 0, ch, TO_VICT);
      } else {
        sprintf(buf, "$e looks to be about %0.1f meters tall and "
                "appears to barely weigh anything.", height);
        act(buf, FALSE, i, 0, ch, TO_VICT);
      }
    }
    diag_char_to_char(i, ch);
    if (IS_DUAL(ch) || IS_ASTRAL(ch)) {
      bool dual = TRUE, init = TRUE;
      if (GET_GRADE(i)) {
        int targ = MAX(1, GET_GRADE(ch) - GET_GRADE(i));
        if (GET_METAMAGIC(i, META_MASKING)) {
          if (!GET_GRADE(ch) || success_test(GET_MAG(ch) / 100, GET_MAG(i) / 100) < targ) {
            if (IS_SET(GET_MASKING(i), MASK_INIT) || IS_SET(GET_MASKING(i), MASK_COMPLETE))
              init = FALSE;
            if (IS_SET(GET_MASKING(i), MASK_DUAL) || IS_SET(GET_MASKING(i), MASK_COMPLETE))
              dual = FALSE;
          }
        }
        if (init) {
          sprintf(buf, "%s aura is ", CAP(HSHR(i)));
          if (GET_GRADE(i) > 6)
            strcat(buf, "^Wblindingly bright^n.");
          else if (GET_GRADE(i) > 2)
            strcat(buf, "^Wshining bright^n.");
          else strcat(buf, "^Wsomewhat bright^n.");
          strcat(buf, "\r\n");
          send_to_char(buf, ch);
        }
      }
      if (IS_DUAL(i) && dual)
        send_to_char(ch, "%s is dual.\r\n", CAP(HSSH(i)));
    }
  }
  if (!IS_NPC(i) && GET_LEVEL(ch) > LVL_MORTAL && i->player.background)
    send_to_char(ch, "Background:\r\n%s\r\n", i->player.background);
  found = FALSE;
  for (j = 0; !found && j < NUM_WEARS; j++)
    if (GET_EQ(i, j) && CAN_SEE_OBJ(ch, GET_EQ(i, j)))
      found = TRUE;
  
  if (found)
  {
    if (ch == i)
      send_to_char("\r\nYou are using:\r\n", ch);
    else
      act("\r\n$n is using:", FALSE, i, 0, ch, TO_VICT);
    for (j = 0; j < NUM_WEARS; j++)
      if (GET_EQ(i, j) && CAN_SEE_OBJ(ch, GET_EQ(i, j))) {
        if (GET_OBJ_TYPE(GET_EQ(i, j)) == ITEM_HOLSTER && GET_OBJ_VAL(GET_EQ(i, j), 0) == 2) {
          send_to_char(where[j], ch);
          show_obj_to_char(GET_EQ(i, j), ch, 7);
        } else if (j == WEAR_WIELD || j == WEAR_HOLD) {
          if (IS_OBJ_STAT(GET_EQ(i, j), ITEM_TWOHANDS))
            send_to_char(hands[2], ch);
          else if (j == WEAR_WIELD)
            send_to_char(hands[(int)i->char_specials.saved.left_handed], ch);
          else
            send_to_char(hands[!i->char_specials.saved.left_handed], ch);
          show_obj_to_char(GET_EQ(i, j), ch, 1);
        } else if ((j == WEAR_BODY || j == WEAR_LARM || j == WEAR_RARM || j == WEAR_WAIST)
                   && GET_EQ(i, WEAR_ABOUT)) {
          if (success_test(GET_INT(ch) + GET_POWER(ch, ADEPT_IMPROVED_PERCEPT), 4 + GET_OBJ_VAL(GET_EQ(i, WEAR_ABOUT), 7)) >= 2) {
            send_to_char(where[j], ch);
            show_obj_to_char(GET_EQ(i, j), ch, 1);
          }
        } else if (j == WEAR_UNDER && (GET_EQ(i, WEAR_ABOUT) || GET_EQ(i, WEAR_BODY))) {
          if (success_test(GET_INT(ch) + GET_POWER(ch, ADEPT_IMPROVED_PERCEPT), 6 +
                           (GET_EQ(i, WEAR_ABOUT) ? GET_OBJ_VAL(GET_EQ(i, WEAR_ABOUT), 7) : 0) +
                           (GET_EQ(i, WEAR_BODY) ? GET_OBJ_VAL(GET_EQ(i, WEAR_BODY), 7) : 0)) >= 2) {
            send_to_char(where[j], ch);
            show_obj_to_char(GET_EQ(i, j), ch, 1);
          }
        } else if (j == WEAR_LEGS && GET_EQ(i, WEAR_ABOUT)) {
          if (success_test(GET_INT(ch) + GET_POWER(ch, ADEPT_IMPROVED_PERCEPT), 2 + GET_OBJ_VAL(GET_EQ(i, WEAR_ABOUT), 7)) >= 2) {
            send_to_char(where[j], ch);
            show_obj_to_char(GET_EQ(i, j), ch, 1);
          }
        } else if ((j == WEAR_RANKLE || j == WEAR_LANKLE) && (GET_EQ(i, WEAR_ABOUT) || GET_EQ(i, WEAR_LEGS))) {
          if (success_test(GET_INT(ch) + GET_POWER(ch, ADEPT_IMPROVED_PERCEPT), 5 +
                           (GET_EQ(i, WEAR_ABOUT) ? GET_OBJ_VAL(GET_EQ(i, WEAR_ABOUT), 7) : 0) +
                           (GET_EQ(i, WEAR_LEGS) ? GET_OBJ_VAL(GET_EQ(i, WEAR_LEGS), 7) : 0)) >= 2) {
            send_to_char(where[j], ch);
            show_obj_to_char(GET_EQ(i, j), ch, 1);
          }
        } else {
          send_to_char(where[j], ch);
          show_obj_to_char(GET_EQ(i, j), ch, 1);
        }
      }
  }
  
  found = FALSE;
  *buf = '\0';
  *buf2 = '\0';
  for (tmp_obj = i->cyberware; tmp_obj; tmp_obj = tmp_obj->next_content)
    switch (GET_OBJ_VAL(tmp_obj, 0)) {
      case CYB_HANDRAZOR:
      case CYB_HANDBLADE:
      case CYB_HANDSPUR:
      case CYB_FIN:
      case CYB_FOOTANCHOR:
      case CYB_CLIMBINGCLAWS:
        if (GET_OBJ_VAL(tmp_obj, 9)) {
          found = TRUE;
          sprintf(ENDOF(buf), "%s\r\n", GET_OBJ_NAME(tmp_obj));
        } else sprintf(ENDOF(buf2), "%s\r\n", cyber_types[GET_OBJ_VAL(tmp_obj, 0)]);
        break;
      case CYB_TORSO:
      case CYB_SKULL:
        if (GET_OBJ_VAL(tmp_obj, 3)) {
          found = TRUE;
          sprintf(ENDOF(buf), "%s\r\n", GET_OBJ_NAME(tmp_obj));
        } else sprintf(ENDOF(buf2), "%s\r\n", cyber_types[GET_OBJ_VAL(tmp_obj, 0)]);
        break;
      case CYB_DATAJACK:
        if (GET_EQ(i, WEAR_HEAD))
          continue;
        if (GET_OBJ_VAL(tmp_obj, 3)) {
          found = TRUE;
          sprintf(ENDOF(buf), "%s\r\n", GET_OBJ_NAME(tmp_obj));
        } else sprintf(ENDOF(buf2), "%s\r\n", cyber_types[GET_OBJ_VAL(tmp_obj, 0)]);
        break;
      case CYB_CHIPJACK:
        if (GET_EQ(i, WEAR_HEAD))
          continue;
        // fall through.
      case CYB_DERMALPLATING:
      case CYB_BALANCETAIL:
        sprintf(ENDOF(buf2), "%s\r\n", cyber_types[GET_OBJ_VAL(tmp_obj, 0)]);
        break;
      case CYB_EYES:
        if (GET_EQ(i, WEAR_EYES) || (GET_EQ(i, WEAR_HEAD) && GET_OBJ_VAL(GET_EQ(i, WEAR_HEAD), 7) > 1))
          continue;
        if ((IS_SET(GET_OBJ_VAL(tmp_obj, 3), EYE_OPTMAG1) || IS_SET(GET_OBJ_VAL(tmp_obj, 3), EYE_OPTMAG2) ||
             IS_SET(GET_OBJ_VAL(tmp_obj, 3), EYE_OPTMAG3)) && success_test(GET_INT(ch), 9) > 0)
          sprintf(ENDOF(buf2), "Optical Magnification\r\n");
        if (IS_SET(GET_OBJ_VAL(tmp_obj, 3), EYE_COSMETIC))
          sprintf(ENDOF(buf2), "%s\r\n", GET_OBJ_NAME(tmp_obj));
        break;
      default:
        found = TRUE;
        sprintf(ENDOF(buf), "%s\r\n", GET_OBJ_NAME(tmp_obj));
        break;
    }
  if (*buf2)
    send_to_char(ch, "\r\nVisible Cyberware:\r\n%s", buf2);
  if (found && GET_LEVEL(ch) > 1)
    send_to_char(ch, "\r\nInternal Cyberware:\r\n%s", buf);
  
  for (tmp_obj = i->bioware; tmp_obj; tmp_obj = tmp_obj->next_content)
    if (GET_OBJ_VAL(tmp_obj, 0) == BIO_ORTHOSKIN) {
      int targ = 10;
      switch (GET_OBJ_VAL(tmp_obj, 1)) {
        case 2:
          targ = 9;
          break;
        case 3:
          targ = 8;
      }
      if (success_test(GET_INT(ch), targ) > 0)
        send_to_char(ch, "\r\nVisible bioware:\r\n%s\r\n", bio_types[GET_OBJ_VAL(tmp_obj, 0)]);
    }
  
  if (GET_LEVEL(ch) >= LVL_BUILDER && i->bioware) {
    send_to_char("\r\nInternal bioware:\r\n", ch);
    for (tmp_obj = i->bioware; tmp_obj; tmp_obj = tmp_obj->next_content)
      send_to_char(ch, "%s\r\n", GET_OBJ_NAME(tmp_obj));
  }
  
  if (ch != i && (GET_REAL_LEVEL(ch) >= LVL_BUILDER || !AWAKE(i)))
  {
    found = FALSE;
    act("\r\nYou peek at $s inventory:", FALSE, i, 0, ch, TO_VICT);
    for (tmp_obj = i->carrying; tmp_obj; tmp_obj = tmp_obj->next_content) {
      if (CAN_SEE_OBJ(ch, tmp_obj)) {
        show_obj_to_char(tmp_obj, ch, 1);
        found = TRUE;
      }
    }
    
    if (!found)
      send_to_char("You can't see anything.\r\n", ch);
  }
}

void list_one_char(struct char_data * i, struct char_data * ch)
{
  struct obj_data *obj = NULL;
  if (IS_NPC(i) && i->player.physical_text.room_desc &&
      GET_POS(i) == GET_DEFAULT_POS(i) && !MOB_FLAGGED(i, MOB_FLAMEAURA))
  {
    if (IS_AFFECTED(i, AFF_INVISIBLE))
      strcpy(buf, "*");
    else
      *buf = '\0';
    if (IS_ASTRAL(ch) || IS_DUAL(ch)) {
      if (IS_ASTRAL(i))
        strcat(buf, "(astral) ");
      else if (IS_DUAL(i) && IS_NPC(i))
        strcat(buf, "(dual) ");
    }
    if (AFF_FLAGGED(i, AFF_MANIFEST) && !(IS_ASTRAL(ch) || IS_DUAL(ch)))
      strcat(buf, "The ghostly image of ");
    strcat(buf, i->player.physical_text.room_desc);
    
    if (DISPLAY_HELPFUL_STRINGS_FOR_MOB_FUNCS) {
      if (mob_index[GET_MOB_RNUM(i)].func) {
        if (mob_index[GET_MOB_RNUM(i)].func == trainer) {
          sprintf(ENDOF(buf), "^y...%s looks willing to train you.^n\r\n", HSSH(i));
        }
        if (mob_index[GET_MOB_RNUM(i)].func == teacher) {
          sprintf(ENDOF(buf), "^y...%s looks willing to help you practice your skills.^n\r\n", HSSH(i));
        }
        if (mob_index[GET_MOB_RNUM(i)].func == metamagic_teacher) {
          // Mundanes can't see metamagic teachers' abilities.
          if (GET_TRADITION(ch) != TRAD_MUNDANE)
            sprintf(ENDOF(buf), "^y...%s looks willing to help you train in metamagic techniques.^n\r\n", HSSH(i));
        }
        if (mob_index[GET_MOB_RNUM(i)].func == adept_trainer) {
          // Adepts can't see adept trainers' abilities.
          if (GET_TRADITION(ch) == TRAD_ADEPT)
            sprintf(ENDOF(buf), "^y...%s looks willing to help you train your powers.^n\r\n", HSSH(i));
        }
        if (mob_index[GET_MOB_RNUM(i)].func == spell_trainer) {
          // Mundanes and adepts can't see spell trainers' abilities.
          if (GET_TRADITION(ch) != TRAD_MUNDANE && GET_TRADITION(ch) != TRAD_ADEPT)
            sprintf(ENDOF(buf), "^y...%s looks willing to help you learn new spells.^n\r\n", HSSH(i));
        }
        if (mob_index[GET_MOB_RNUM(i)].func == johnson) {
          sprintf(ENDOF(buf), "^y...%s might have a job for you.^n\r\n", HSSH(i));
        }
        if (mob_index[GET_MOB_RNUM(i)].func == shop_keeper) {
          sprintf(ENDOF(buf), "^y...%s has a few things for sale.^n\r\n", HSSH(i));
        }
        if (mob_index[GET_MOB_RNUM(i)].func == landlord_spec) {
          sprintf(ENDOF(buf), "^y...%s might have some rooms for rent.^n\r\n", HSSH(i));
        }
      }
      
      if (mob_index[GET_MOB_RNUM(i)].sfunc) {
        if (mob_index[GET_MOB_RNUM(i)].sfunc == trainer) {
          sprintf(ENDOF(buf), "^y...%s%s looks willing to train you.^n\r\n", HSSH(i), mob_index[GET_MOB_RNUM(i)].func ? " also" : "");
        }
        if (mob_index[GET_MOB_RNUM(i)].sfunc == teacher) {
          sprintf(ENDOF(buf), "^y...%s%s looks willing to help you practice your skills.^n\r\n", HSSH(i), mob_index[GET_MOB_RNUM(i)].func ? " also" : "");
        }
        if (mob_index[GET_MOB_RNUM(i)].sfunc == metamagic_teacher) {
          // Mundanes can't see metamagic teachers' abilities.
          if (GET_TRADITION(ch) != TRAD_MUNDANE)
            sprintf(ENDOF(buf), "^y...%s%s looks willing to help you train in metamagic techniques.^n\r\n", HSSH(i), mob_index[GET_MOB_RNUM(i)].func ? " also" : "");
        }
        if (mob_index[GET_MOB_RNUM(i)].sfunc == adept_trainer) {
          // Adepts can't see adept trainers' abilities.
          if (GET_TRADITION(ch) == TRAD_ADEPT)
            sprintf(ENDOF(buf), "^y...%s%s looks willing to help you train your powers.^n\r\n", HSSH(i), mob_index[GET_MOB_RNUM(i)].func ? " also" : "");
        }
        if (mob_index[GET_MOB_RNUM(i)].sfunc == spell_trainer) {
          // Mundanes and adepts can't see spell trainers' abilities.
          if (GET_TRADITION(ch) != TRAD_MUNDANE && GET_TRADITION(ch) != TRAD_ADEPT)
            sprintf(ENDOF(buf), "^y...%s%s looks willing to help you learn new spells.^n\r\n", HSSH(i), mob_index[GET_MOB_RNUM(i)].func ? " also" : "");
        }
        if (mob_index[GET_MOB_RNUM(i)].sfunc == johnson) {
          sprintf(ENDOF(buf), "^y...%s%s might have a job for you.^n\r\n", HSSH(i), mob_index[GET_MOB_RNUM(i)].func ? " also" : "");
        }
        if (mob_index[GET_MOB_RNUM(i)].sfunc == shop_keeper) {
          sprintf(ENDOF(buf), "^y...%s%s has a few things for sale.^n\r\n", HSSH(i), mob_index[GET_MOB_RNUM(i)].func ? " also" : "");
        }
        if (mob_index[GET_MOB_RNUM(i)].sfunc == landlord_spec) {
          sprintf(ENDOF(buf), "^y...%s might have some rooms for rent.^n\r\n", HSSH(i));
        }
      }
    }
    
    send_to_char(buf, ch);
    
    return;
  }
  make_desc(ch, i, buf, FALSE, FALSE);
  if (PRF_FLAGGED(i, PRF_AFK))
    strcat(buf, " (AFK)");
  if (PLR_FLAGGED(i, PLR_SWITCHED))
    strcat(buf, " (switched)");
  if (IS_AFFECTED(i, AFF_INVISIBLE) || IS_AFFECTED(i, AFF_IMP_INVIS) || IS_AFFECTED(i, AFF_SPELLINVIS) || IS_AFFECTED(i, AFF_SPELLIMPINVIS))
    strcat(buf, " (invisible)");
  if (PLR_FLAGGED(ch, PLR_NEWBIE) && !IS_NPC(i))
    strcat(buf, " (player)");
  if (IS_AFFECTED(i, AFF_HIDE))
    strcat(buf, " (hidden)");
  if (!IS_NPC(i) && !i->desc &&
      !PLR_FLAGS(i).AreAnySet(PLR_MATRIX, PLR_PROJECT, PLR_SWITCHED, ENDBIT))
    strcat(buf, " (linkless)");
  if (IS_ASTRAL(ch) || IS_DUAL(ch)) {
    bool dual = TRUE;
    if (IS_ASTRAL(i))
      strcat(buf, " (astral)");
    if (GET_GRADE(i)) {
      bool init = TRUE;
      if (GET_METAMAGIC(i, META_MASKING)) {
        if (IS_SET(GET_MASKING(i), MASK_INIT) || IS_SET(GET_MASKING(i), MASK_COMPLETE))
          init = FALSE;
        if (IS_SET(GET_MASKING(i), MASK_DUAL) || IS_SET(GET_MASKING(i), MASK_COMPLETE))
          dual = FALSE;
      }
      if (init) {
        if (GET_GRADE(i) > 6)
          strcat(buf, " ^W(Blinding Aura)^n");
        else if (GET_GRADE(i) > 2)
          strcat(buf, " ^W(Shining Aura)^n");
        else strcat(buf, " ^W(Bright Aura)^n");
      }
    }
    if (IS_DUAL(i) && dual)
      strcat(buf, " (dual)");
  }
  if (PLR_FLAGGED(i, PLR_WRITING))
    strcat(buf, " (writing)");
  if (PLR_FLAGGED(i, PLR_MAILING))
    strcat(buf, " (mailing)");
  if (PLR_FLAGGED(i, PLR_EDITING))
    strcat(buf, " (editing)");
  if (PLR_FLAGGED(i, PLR_PROJECT))
    strcat(buf, " (projecting)");
  if (IS_NPC(i) && MOB_FLAGGED(i, MOB_FLAMEAURA))
    strcat(buf, ", surrounded by flames,");
  
  if (GET_QUI(i) <= 0)
    strcat(buf, " is here, seemingly paralyzed.");
  else if (PLR_FLAGGED(i, PLR_MATRIX))
    strcat(buf, " is here, jacked into a cyberdeck.");
  else if (PLR_FLAGGED(i, PLR_REMOTE))
    strcat(buf, " is here, jacked into a remote control deck.");
  else if (AFF_FLAGGED(i, AFF_DESIGN) || AFF_FLAGGED(i, AFF_PROGRAM))
    strcat(buf, " is here, typing away at a computer.");
  else if (AFF_FLAGGED(i, AFF_PART_DESIGN) || AFF_FLAGGED(i, AFF_PART_BUILD))
    strcat(buf, " is here, working on a cyberdeck.");
  else if (AFF_FLAGGED(i, AFF_SPELLDESIGN))
    strcat(buf, " is here, working on a spell design.");
  else if (AFF_FLAGGED(i, AFF_CONJURE))
    strcat(buf, " is here, performing a conjuring ritual.");
  else if (AFF_FLAGGED(i, AFF_LODGE))
    strcat(buf, " is here, building a shamanic lodge.");
  else if (AFF_FLAGGED(i, AFF_CIRCLE))
    strcat(buf, " is here, drawing a hermetic circle.");
  else if (AFF_FLAGGED(i, AFF_PACKING))
    strcat(buf, " is here, working on a workshop.");
  else if (AFF_FLAGGED(i, AFF_BONDING))
    strcat(buf, " is here, performing a bonding ritual.");
  else if (AFF_FLAGGED(i, AFF_PRONE))
    strcat(buf, " is laying prone here.");
  else if (AFF_FLAGGED(i, AFF_PILOT))
  {
    if (AFF_FLAGGED(i, AFF_RIG))
      strcat(buf, " is plugged into the dashboard.");
    else
      strcat(buf, " is sitting in the drivers seat.");
  } else if ((obj = get_mount_manned_by_ch(i))) {
      sprintf(buf, "%s is manning %s.", buf, GET_OBJ_NAME(obj));
  } else if (GET_POS(i) != POS_FIGHTING)
  {
    strcat(buf, positions[(int) GET_POS(i)]);
    if (GET_DEFPOS(i))
      sprintf(buf2, ", %s.", GET_DEFPOS(i));
    else
      sprintf(buf2, ".");
    strcat(buf, buf2);
  } else
  {
    if (FIGHTING(i)) {
      if (AFF_FLAGGED(ch, AFF_BANISH))
        strcat(buf, " is here, attempting to banish ");
      else
        strcat(buf, " is here, fighting ");
      if (FIGHTING(i) == ch)
        strcat(buf, "YOU!");
      else {
        if (i->in_room == FIGHTING(i)->in_room)
          strcat(buf, PERS(FIGHTING(i), ch));
        else
          strcat(buf, "someone in the distance");
        strcat(buf, "!");
      }
    } else if (FIGHTING_VEH(i)) {
      strcat(buf, " is here, fighting ");
      if ((ch->in_veh && ch->in_veh == FIGHTING_VEH(i)) || (ch->char_specials.rigging && ch->char_specials.rigging == FIGHTING_VEH(i)))
        strcat(buf, "YOU!");
      else {
        if (i->in_room == FIGHTING_VEH(i)->in_room)
          strcat(buf, GET_VEH_NAME(FIGHTING_VEH(i)));
        else
          strcat(buf, "someone in the distance");
        strcat(buf, "!");
      }
    } else                      /* NIL fighting pointer */
      strcat(buf, " is here struggling with thin air.");
  }
  
  strcat(buf, "\r\n");
  
  send_to_char(buf, ch);
}

void list_char_to_char(struct char_data * list, struct char_data * ch)
{
  struct char_data *i;
  struct veh_data *veh;
  
  // Show vehicle's contents to character.
  if (ch->in_veh && !ch->in_room) {
    for (i = list; i; i = i->next_in_veh) {
      if (CAN_SEE(ch, i) && ch != i && ch->vfront == i->vfront) {
        list_one_char(i, ch);
      }
    }
  }
  
  // Show room's characters to character. Done this way because list_char_to_char should have been split for vehicles but wasn't.
  for (i = list; i; i = i->next_in_room) {
    // Skip them if they're invisible to us, or if they're us and we're not rigging.
    if (!CAN_SEE(ch, i) || !(ch != i || ch->char_specials.rigging) || (ch->in_veh && i->in_veh == ch->in_veh)) {
      continue;
    }
    
    if ((ch->in_veh || (ch->char_specials.rigging))) {
      RIG_VEH(ch, veh);
      
      bool failed = FALSE;
      if (veh->cspeed > SPEED_IDLE) {
        if (get_speed(veh) >= 200) {
          if (!success_test(GET_INT(ch) + GET_POWER(ch, ADEPT_IMPROVED_PERCEPT), 7)) {
            failed = TRUE;
          }
        }
        else if (get_speed(veh) >= 120) {
          if (!success_test(GET_INT(ch) + GET_POWER(ch, ADEPT_IMPROVED_PERCEPT), 6)) {
            failed = TRUE;
          }
        }
        else if (get_speed(veh) >= 60) {
          if (!success_test(GET_INT(ch) + GET_POWER(ch, ADEPT_IMPROVED_PERCEPT), 5)) {
            failed = TRUE;
          }
        }
        else {
          if (!success_test(GET_INT(ch) + GET_POWER(ch, ADEPT_IMPROVED_PERCEPT), 4)) {
            failed = TRUE;
          }
        }
      }
      
      if (failed) {
        continue;
      }
    }
    
    list_one_char(i, ch);
  }
}

void disp_long_exits(struct char_data *ch, bool autom)
{
  int door;
  struct room_data *wasin = NULL;
  struct veh_data *veh;
  *buf = '\0';
  buf2[0] = '\0'; // so strcats will start at the beginning
  
  RIG_VEH(ch, veh);
  if (veh)
  {
    wasin = ch->in_room;
    ch->in_room = get_veh_in_room(veh);
  }
  for (door = 0; door < NUM_OF_DIRS; door++)
  {
    if (EXIT(ch, door) && EXIT(ch, door)->to_room && EXIT(ch, door)->to_room != &world[0]) {
      if (GET_REAL_LEVEL(ch) >= LVL_BUILDER) {
        sprintf(buf2, "%-5s - [%5ld] %s%s\r\n", dirs[door],
                EXIT(ch, door)->to_room->number,
                EXIT(ch, door)->to_room->name,
                (IS_SET(EXIT(ch, door)->exit_info, EX_CLOSED) ? " (closed)" : ""));
        if (autom)
          strcat(buf, "^c");
        strcat(buf, CAP(buf2));
      } else if (!IS_SET(EXIT(ch, door)->exit_info, EX_HIDDEN)) {
        sprintf(buf2, "%-5s - ", dirs[door]);
        if (!IS_ASTRAL(ch) &&
            !LIGHT_OK(ch))
          strcat(buf2, "Too dark to tell.\r\n");
        else {
          if (IS_SET(EXIT(ch, door)->exit_info, EX_CLOSED)) {
            SPECIAL(call_elevator);
            SPECIAL(elevator_spec);
            // If it leads into an elevator shaft or car from a landing, it's an elevator door.
            // If it leads into a landing from an elevator shaft or car, it's an elevator door.
            if (EXIT(ch, door)->to_room && (((ROOM_FLAGGED(EXIT(ch, door)->to_room, ROOM_ELEVATOR_SHAFT) || EXIT(ch, door)->to_room->func == elevator_spec)
                                              && get_ch_in_room(ch)->func == call_elevator)
                                            || ((ROOM_FLAGGED(get_ch_in_room(ch), ROOM_ELEVATOR_SHAFT) || get_ch_in_room(ch)->func == elevator_spec)
                                              && EXIT(ch, door)->to_room->func == call_elevator)
                                           )) {
                strcat(buf2, "A pair of closed elevator doors");
            }
            else
              sprintf(ENDOF(buf2), "A closed %s", *(fname(EXIT(ch, door)->keyword)) ? fname(EXIT(ch, door)->keyword) : "door");
          } else
            strcat(buf2, EXIT(ch, door)->to_room->name);
          strcat(buf2, "\r\n");
        }
        if (autom)
          strcat(buf, "^c");
        strcat(buf, CAP(buf2));
      }
    }
  }
  
  if (autom)
    send_to_char("^cObvious exits:\r\n", ch);
  else
    send_to_char("Obvious exits:\r\n", ch);
  if (*buf)
    send_to_char(buf, ch);
  else
    send_to_char(" None.\r\n", ch);
  
  if (veh)
    ch->in_room = wasin;
  
}
void do_auto_exits(struct char_data * ch)
{
  int door;
  struct veh_data *veh;
  *buf = '\0';
  
  if (PRF_FLAGGED(ch, PRF_LONGEXITS))
  {
    disp_long_exits(ch, TRUE);
  } else
  {
    for (door = 0; door < NUM_OF_DIRS; door++)
      if (EXIT(ch, door) && EXIT(ch, door)->to_room && EXIT(ch, door)->to_room != &world[0]) {
        if (ch->in_veh || ch->char_specials.rigging) {
          RIG_VEH(ch, veh);
          if (!ROOM_FLAGGED(EXIT(veh, door)->to_room, ROOM_ROAD) &&
              !ROOM_FLAGGED(EXIT(veh, door)->to_room, ROOM_GARAGE) &&
              !IS_SET(EXIT(ch, door)->exit_info, EX_HIDDEN))
            sprintf(ENDOF(buf), "(%s) ", exitdirs[door]);
          else if (!IS_SET(EXIT(ch, door)->exit_info, EX_CLOSED | EX_HIDDEN))
            sprintf(ENDOF(buf), "%s ", exitdirs[door]);
        } else {
          if (!IS_SET(EXIT(ch, door)->exit_info, EX_HIDDEN) || GET_LEVEL(ch) > LVL_MORTAL) {
            if (IS_SET(EXIT(ch, door)->exit_info, EX_LOCKED))
              sprintf(ENDOF(buf), "%s(L) ", exitdirs[door]);
            else if (IS_SET(EXIT(ch, door)->exit_info, EX_CLOSED))
              sprintf(ENDOF(buf), "%s(C) ", exitdirs[door]);
            else
              sprintf(ENDOF(buf), "%s ", exitdirs[door]);
          }
        }
      }
    sprintf(buf2, "^c[ Exits: %s]^n\r\n", *buf ? buf : "None! ");
    send_to_char(buf2, ch);
  }
}


ACMD(do_exits)
{
  disp_long_exits(ch, FALSE);
}

void update_blood(void)
{
  int i;
  extern rnum_t top_of_world;
  
  for (i = 0; i < top_of_world; i++) {
    if (world[i].blood > 0) {
      world[i].blood--;
      if (!ROOM_FLAGGED(&world[i], ROOM_INDOORS)) {
        if (weather_info.sky == SKY_RAINING) {
          world[i].blood--;
          sprintf(buf, "%s the blood staining this area washes away in the rain.", world[i].blood > 3 ? "Some of" : (world[i].blood > 1 ? "Most of" : "The last of"));
          send_to_room(buf, &world[i]);
        } else if (weather_info.sky == SKY_LIGHTNING) {
          world[i].blood -= 2;
          sprintf(buf, "%s the blood staining this area washes away in the pounding rain.", world[i].blood > 4 ? "Some of" : (world[i].blood > 1 ? "Most of" : "The last of"));
          send_to_room(buf, &world[i]);
        }
      }
    }
  }
}

void look_in_veh(struct char_data * ch)
{
  struct room_data *was_in = NULL;
  if (!(AFF_FLAGGED(ch, AFF_RIG) || PLR_FLAGGED(ch, PLR_REMOTE)))
  {
    if (!ch->in_veh->in_room && !ch->in_veh->in_veh) {
      sprintf(buf, "SYSERR: Character %s is not in a room or vehicle.", GET_CHAR_NAME(ch));
      mudlog(buf, ch, LOG_SYSLOG, TRUE);
      send_to_char("ALERT AN IMM!!\r\n", ch);
      return;
    }
    send_to_char(ch, "^CInside %s^n\r\n", GET_VEH_NAME(ch->in_veh), ch);
    send_to_char(ch, ch->vfront ? ch->in_veh->inside_description : ch->in_veh->rear_description);
    CCHAR = "^g";
    CGLOB = KGRN;
    list_obj_to_char(ch->in_veh->contents, ch, 0, FALSE, FALSE);
    CGLOB = KNRM;
    CCHAR = NULL;
    list_obj_to_char(ch->in_veh->contents, ch, 0, FALSE, TRUE);
    list_char_to_char(ch->in_veh->people, ch);
    if (!ch->vfront) {
      CCHAR = "^y";
      list_veh_to_char(ch->in_veh->carriedvehs, ch);
    }
  }
  if (!ch->in_room || PLR_FLAGGED(ch, PLR_REMOTE))
  {
    struct veh_data *veh;
    RIG_VEH(ch, veh);
    if (!veh->in_room && !veh->in_veh) {
      sprintf(buf, "SYSERR: Vehicle %s is not in a room or vehicle.", GET_VEH_NAME(veh));
      mudlog(buf, ch, LOG_SYSLOG, TRUE);
      send_to_char("ALERT AN IMM!!\r\n", ch);
      return;
    }
    if (veh->in_veh) {
      int ov = ch->vfront;
      ch->vfront = FALSE;
      send_to_char(ch, "^CInside %s^n\r\n", GET_VEH_NAME(veh->in_veh), ch);
      send_to_char(ch, veh->in_veh->rear_description);
      CCHAR = "^g";
      CGLOB = KGRN;
      list_obj_to_char(veh->in_veh->contents, ch, 0, FALSE, FALSE);
      CGLOB = KNRM;
      CCHAR = NULL;
      list_obj_to_char(veh->in_veh->contents, ch, 0, FALSE, TRUE);
      list_char_to_char(veh->in_veh->people, ch);
      ch->vfront = ov;
    } else {
      send_to_char(ch, "\r\n^CAround you is %s\r\n", veh->in_room->name);
      if (get_speed(veh) <= 200) {
        if (veh->in_room->night_desc && weather_info.sunlight == SUN_DARK)
          send_to_char(ch, veh->in_room->night_desc);
        else
          send_to_char(ch, veh->in_room->description);
      }
      if (PLR_FLAGGED(ch, PLR_REMOTE))
        was_in = ch->in_room;
      ch->in_room = veh->in_room;
      do_auto_exits(ch);
      CCHAR = "^g";
      CGLOB = KGRN;
      list_obj_to_char(veh->in_room->contents, ch, 0, FALSE, FALSE);
      CGLOB = KNRM;
      CCHAR = NULL;
      list_char_to_char(veh->in_room->people, ch);
      CCHAR = "^y";
      list_veh_to_char(veh->in_room->vehicles, ch);
      if (PLR_FLAGGED(ch, PLR_REMOTE))
        ch->in_room = was_in;
      else
        ch->in_room = NULL;
    }
  }
}

void look_at_room(struct char_data * ch, int ignore_brief)
{
  if (!LIGHT_OK(ch)) {
    send_to_char("It is pitch black...\r\n", ch);
    return;
  }
  
  if (ch->in_veh && (!ch->in_room || PLR_FLAGGED(ch, PLR_REMOTE))) {
    look_in_veh(ch);
    return;
  }
  
  if (!ch->in_room) {
    send_to_char("Your mind is blasted by the eldritch horrors of the unknown void you're drifting in.\r\n", ch);
    sprintf(buf, "SYSERR: %s tried to look at their room but is nowhere!", GET_CHAR_NAME(ch));
    mudlog(buf, ch, LOG_SYSLOG, TRUE);
    return;
  }
  
  if ((PRF_FLAGGED(ch, PRF_ROOMFLAGS) && GET_REAL_LEVEL(ch) >= LVL_BUILDER)) {
    ROOM_FLAGS(ch->in_room).PrintBits(buf, MAX_STRING_LENGTH, room_bits, ROOM_MAX);
    sprintf(buf2, "^C[%5ld] %s [ %s ]^n\r\n", GET_ROOM_VNUM(ch->in_room), GET_ROOM_NAME(ch->in_room), buf);
    send_to_char(buf2, ch);
  } else
    send_to_char(ch, "^C%s^n\r\n", GET_ROOM_NAME(ch->in_room), ch);
  
  // TODO: Why is this code here? If you're in a vehicle, you do look_in_veh() above right?
  if (!(ch->in_veh && get_speed(ch->in_veh) > 200)) {
    if (ch->in_room->night_desc && weather_info.sunlight == SUN_DARK)
      send_to_char(ch->in_room->night_desc, ch);
    else
      send_to_char(ch->in_room->description, ch);
  }
  
  /* autoexits */
  if (PRF_FLAGGED(ch, PRF_AUTOEXIT))
    do_auto_exits(ch);
  
  if (ch->in_room->blood > 0)
    send_to_char(blood_messages[(int) ch->in_room->blood], ch);
  if (GET_BACKGROUND_COUNT(ch->in_room) && (IS_ASTRAL(ch) || IS_DUAL(ch))) {
    if (GET_BACKGROUND_AURA(ch->in_room) == AURA_POWERSITE) {
      switch (GET_BACKGROUND_COUNT(ch->in_room)) {
        case 1:
          send_to_char("^CAstral energy seems to be slightly concentrated here.^n\r\n", ch);
          break;
        case 2:
          send_to_char("^CAstral energy seems to be mildly concentrated here.^n\r\n", ch);
          break;
        case 3:
          send_to_char("^CAstral energy seems to be concentrated here.^n\r\n", ch);
          break;
        case 4:
          send_to_char("^CAstral energy seems to be heavily concentrated here.^n\r\n", ch);
          break;
        case 5:
          send_to_char("^CThe power of the astral energy here is nearly overwhelming.^n\r\n", ch);
          break;
        default:
          send_to_char("^RA horrific quantity of astral energy tears at your senses.^n\r\n", ch);
          sprintf(buf, "SYSERR: '%s' (%ld) has powersite background count %d, which is greater than displayable max of 5.",
                  ch->in_room->name, ch->in_room->number, GET_BACKGROUND_COUNT(ch->in_room));
          mudlog(buf, ch, LOG_SYSLOG, TRUE);
          break;
      }
    } else if (GET_BACKGROUND_COUNT(ch->in_room) < 6) {
      sprintf(buf, "^cA");
      switch (GET_BACKGROUND_COUNT(ch->in_room)) {
        case 1:
          strcat(buf, " distracting");
          break;
        case 2:
          strcat(buf, " light");
          break;
        case 3:
          strcat(buf, " heavy");
          break;
        case 4:
          strcat(buf, "n almost overwhelming");
          break;
        case 5:
          strcat(buf, "n overwhelming");
          break;
      }
      sprintf(ENDOF(buf), " aura of %s pervades the area.^n\r\n", background_types[GET_BACKGROUND_AURA(ch->in_room)]);
      send_to_char(buf, ch);
    } else {
      send_to_char("^RThe mana is warping here!^n\r\n", ch);
    }
  }
  if (ch->in_room->vision[1]) {
    switch (ch->in_room->vision[1]) {
      case LIGHT_GLARE:
        send_to_char("^CThe light is glaring off of everything in here.\r\n", ch);
        break;
      case LIGHT_MIST:
        send_to_char("^LA light mist fills the area.\r\n", ch);
        break;
      case LIGHT_LIGHTSMOKE:
      case LIGHT_THERMALSMOKE:
        send_to_char("^LSmoke drifts through the area, obscuring your view.\r\n", ch);
        break;
      case LIGHT_HEAVYSMOKE:
        send_to_char("^LThe air is thick with smoke.\r\n", ch);
        break;
    }
  }
  if (!ROOM_FLAGGED(ch->in_room, ROOM_INDOORS)) {
    if (IS_WATER(ch->in_room)) {
      if (weather_info.sky >= SKY_RAINING) {
        send_to_char(ch, "^cThe water around you is dimpled by the falling rain.^n\r\n");
      }
    } else {
      if (weather_info.sky >= SKY_RAINING) {
        send_to_char(ch, "^cRain splashes into the puddles around your feet.^n\r\n");
      } else if (weather_info.lastrain < 5) {
        send_to_char(ch, "^cThe ground is wet, it must have rained recently.^n\r\n");
      }
    }
  }
  if (ch->in_room->poltergeist[0])
    send_to_char("^cAn invisible force is whipping small objects around the area.^n\r\n", ch);
  if (ch->in_room->icesheet[0])
    send_to_char("^CIce covers the floor.^n\r\n", ch);
  
  // Is there an elevator car here?
  if (ROOM_FLAGGED(ch->in_room, ROOM_ELEVATOR_SHAFT)) {
    bool match = FALSE;
    // Iterate through elevators to find one that contains this shaft.
    for (int index = 0; !match && index < num_elevators; index++) {
      // Iterate through floors to see if a given floor matches the shaft's vnum.
      for (int floor = 0; !match && floor < elevator[index].num_floors; floor++) {
        // Check for a match.
        if (elevator[index].floor[floor].shaft_vnum == ch->in_room->number) {
          // Check for the car being at this floor.
          if (world[real_room(elevator[index].room)].rating == floor)
            send_to_char("^RThe massive bulk of an elevator car fills the hoistway, squeezing you aside.^n\r\n", ch);
          match = TRUE;
        }
      }
    }
  }
  
  /* now list characters & objects */
  // what fun just to get a colorized listing
  CCHAR = "^g";
  CGLOB = KGRN;
  list_obj_to_char(ch->in_room->contents, ch, 0, FALSE, FALSE);
  CGLOB = KNRM;
  CCHAR = NULL;
  list_obj_to_char(ch->in_room->contents, ch, 0, FALSE, TRUE);
  list_char_to_char(ch->in_room->people, ch);
  CCHAR = "^y";
  list_veh_to_char(ch->in_room->vehicles, ch);
}

void look_in_direction(struct char_data * ch, int dir)
{
  if (EXIT(ch, dir))
  {
    if (IS_SET(EXIT(ch, dir)->exit_info, EX_HIDDEN)) {
      if (!success_test(GET_INT(ch) + GET_POWER(ch, ADEPT_IMPROVED_PERCEPT), EXIT(ch, dir)->hidden)) {
        send_to_char("You see nothing special.\r\n", ch);
        return;
      } else {
        REMOVE_BIT(EXIT(ch, dir)->exit_info, EX_HIDDEN);
        send_to_char("You discover an exit...\r\n", ch);
      }
    }
    
    if (EXIT(ch, dir)->general_description)
      send_to_char(EXIT(ch, dir)->general_description, ch);
    else
      send_to_char("You see nothing special.\r\n", ch);
    
    if (IS_SET(EXIT(ch, dir)->exit_info, EX_DESTROYED) && EXIT(ch, dir)->keyword)
      send_to_char(ch, "The %s is destroyed.\r\n", fname(EXIT(ch, dir)->keyword), !strcmp(fname(EXIT(ch, dir)->keyword), "doors") ? "are" : "is");
    else if (IS_SET(EXIT(ch, dir)->exit_info, EX_CLOSED) && EXIT(ch, dir)->keyword)
      send_to_char(ch, "The %s %s closed.\r\n", fname(EXIT(ch, dir)->keyword), !strcmp(fname(EXIT(ch, dir)->keyword), "doors") ? "are" : "is");
    else if (IS_SET(EXIT(ch, dir)->exit_info, EX_ISDOOR) && EXIT(ch, dir)->keyword)
      send_to_char(ch, "The %s is open.\r\n", fname(EXIT(ch, dir)->keyword), !strcmp(fname(EXIT(ch, dir)->keyword), "doors") ? "are" : "is");
    
    if (ROOM_FLAGGED(ch->in_room, ROOM_HOUSE)){
      /* Apartments have peepholes. */
      struct room_data *original_loc = ch->in_room;
      struct room_data *targ_loc = EXIT(ch, dir)->to_room;
      send_to_char("Through the peephole, you see:\r\n", ch);
      
      char_from_room(ch);
      char_to_room(ch, targ_loc);
      look_at_room(ch, 0);
      char_from_room(ch);
      char_to_room(ch, original_loc);
    }
  } else
    send_to_char("You see nothing special.\r\n", ch);
}

void look_in_obj(struct char_data * ch, char *arg, bool exa)
{
  struct obj_data *obj = NULL;
  struct char_data *dummy = NULL;
  struct veh_data *veh = NULL;
  int amt, bits;
  
  if (!*arg) {
    send_to_char("Look in what?\r\n", ch);
    return;
  }
  
  // Find the specified thing. Vehicle will take priority over object.
  if (ch->in_veh) {
    bits = generic_find(arg, FIND_OBJ_INV | FIND_OBJ_ROOM | FIND_OBJ_EQUIP, ch, &dummy, &obj);
    veh = get_veh_list(arg, ch->in_veh->carriedvehs, ch);
  } else {
    bits = generic_find(arg, FIND_OBJ_INV | FIND_OBJ_ROOM | FIND_OBJ_EQUIP, ch, &dummy, &obj);
    veh = get_veh_list(arg, ch->in_room->vehicles, ch);
  }
  
  if (!bits && !veh) {
    sprintf(buf, "There doesn't seem to be %s %s here.\r\n", AN(arg), arg);
    send_to_char(buf, ch);
    return;
  }
  
  if (veh) {
    struct veh_data *curr_in_veh = ch->in_veh;
    bool curr_vfront = ch->vfront;
    
    if (veh->cspeed > SPEED_IDLE) {
      if (success_test(GET_INT(ch) + GET_POWER(ch, ADEPT_IMPROVED_PERCEPT), 8)) {
        ch->in_veh = veh;
        ch->vfront = TRUE;
        look_in_veh(ch);
      } else {
        send_to_char(ch, "It's moving too fast for you to get a good look inside.\r\n");
        return;
      }
    } else {
      for (struct char_data *vict = veh->people; vict; vict = vict->next_in_veh)
        act("$n peers inside.", FALSE, ch, 0, vict, TO_VICT);
      ch->in_veh = veh;
      ch->vfront = TRUE;
      look_in_veh(ch);
    }
    ch->in_veh = curr_in_veh;
    curr_vfront = ch->vfront;
    return;
  } else if ((GET_OBJ_TYPE(obj) != ITEM_DRINKCON) &&
             (GET_OBJ_TYPE(obj) != ITEM_FOUNTAIN) &&
             (GET_OBJ_TYPE(obj) != ITEM_CONTAINER) &&
             (GET_OBJ_TYPE(obj) != ITEM_QUIVER) &&
             (GET_OBJ_TYPE(obj) != ITEM_HOLSTER) &&
             (GET_OBJ_TYPE(obj) != ITEM_WORN) &&
             (GET_OBJ_TYPE(obj) != ITEM_KEYRING))
    send_to_char("There's nothing inside that!\r\n", ch);
  else
  {
    if (GET_OBJ_TYPE(obj) == ITEM_WORN) {
      if (obj->contains) {
        send_to_char(GET_OBJ_NAME(obj), ch);
        switch (bits) {
          case FIND_OBJ_INV:
            send_to_char(" (carried): \r\n", ch);
            break;
          case FIND_OBJ_ROOM:
            send_to_char(" (here): \r\n", ch);
            break;
          case FIND_OBJ_EQUIP:
            send_to_char(" (used): \r\n", ch);
            break;
        }
        list_obj_to_char(obj->contains, ch, 2, TRUE, FALSE);
      }
    } else if (GET_OBJ_TYPE(obj) == ITEM_CONTAINER || GET_OBJ_TYPE(obj) == ITEM_HOLSTER ||
               GET_OBJ_TYPE(obj) == ITEM_QUIVER || GET_OBJ_TYPE(obj) == ITEM_KEYRING) {
      if (IS_SET(GET_OBJ_VAL(obj, 1), CONT_CLOSED)) {
        send_to_char("It is closed.\r\n", ch);
        return;
      } else {
        if (!exa) {
          send_to_char(GET_OBJ_NAME(obj), ch);
          switch (bits) {
            case FIND_OBJ_INV:
              send_to_char(" (carried): \r\n", ch);
              break;
            case FIND_OBJ_ROOM:
              send_to_char(" (here): \r\n", ch);
              break;
            case FIND_OBJ_EQUIP:
              send_to_char(" (used): \r\n", ch);
              break;
          }
        }
        list_obj_to_char(obj->contains, ch, 2, TRUE, FALSE);
      }
    } else {            /* item must be a fountain or drink container */
      if (GET_OBJ_VAL(obj, 1) <= 0)
        send_to_char("It is empty.\r\n", ch);
      else {
        amt = ((GET_OBJ_VAL(obj, 1) * 3) / MAX(1, GET_OBJ_VAL(obj, 0)));
        sprintf(buf, "It's %sfull of a %s liquid.\r\n", fullness[amt],
                color_liquid[GET_OBJ_VAL(obj, 2)]);
        send_to_char(buf, ch);
      }
    }
  }
}

char *find_exdesc(char *word, struct extra_descr_data * list)
{
  struct extra_descr_data *i;
  
  for (i = list; i; i = i->next)
    if (isname(word, i->keyword))
      return (i->description);
  
  return NULL;
}

/*
 * Given the argument "look at <target>", figure out what object or char
 * matches the target.  First, see if there is another char in the room
 * with the name.  Then check local objs for exdescs.
 */
void look_at_target(struct char_data * ch, char *arg)
{
  int bits, found = 0, j;
  struct room_data *was_in = ch->in_room;
  struct char_data *found_char = NULL;
  struct obj_data *obj = NULL, *found_obj = NULL;
  struct veh_data *found_veh = NULL;
  char *desc;
  
  if (!*arg)
  {
    send_to_char("Look at what?\r\n", ch);
    return;
  }
  if (ch->char_specials.rigging)
    if (ch->char_specials.rigging->type == VEH_DRONE)
      ch->in_room = get_veh_in_room(ch->char_specials.rigging);
  
  bits = generic_find(arg, FIND_OBJ_INV | FIND_OBJ_ROOM | FIND_OBJ_EQUIP |
                      FIND_CHAR_ROOM, ch, &found_char, &found_obj);
  
  if ((!str_cmp(arg, "self") || !str_cmp(arg, "me") || !str_cmp(arg, "myself"))) {
    if (AFF_FLAGGED(ch, AFF_RIG))
    {
      send_to_char(GET_VEH_DESC(ch->in_veh), ch);
      return;
    } else if (PLR_FLAGGED(ch, PLR_REMOTE))
    {
      send_to_char(GET_VEH_DESC(ch->char_specials.rigging), ch);
      ch->in_room = was_in;
      return;
    }
  }
  
  if (!ch->in_veh || (ch->in_veh && !ch->vfront))
  {
    found_veh = get_veh_list(arg, ch->in_veh ? ch->in_veh->carriedvehs : ch->in_room->vehicles, ch);
    if (found_veh) {
      send_to_char(GET_VEH_DESC(found_veh), ch);
      if (PLR_FLAGGED(ch, PLR_REMOTE))
        ch->in_room = was_in;
      return;
    }
  }
  /* Is the target a character? */
  if (found_char != NULL)
  {
    look_at_char(found_char, ch);
    if (ch != found_char && !ch->char_specials.rigging) {
      if (CAN_SEE(found_char, ch))
        act("$n looks at you.", TRUE, ch, 0, found_char, TO_VICT);
      act("$n looks at $N.", TRUE, ch, 0, found_char, TO_NOTVICT);
    }
    ch->in_room = was_in;
    return;
  } else if (ch->in_veh)
  {
    found_char = get_char_veh(ch, arg, ch->in_veh);
    if (found_char) {
      look_at_char(found_char, ch);
      if (ch != found_char) {
        if (CAN_SEE(found_char, ch)) {
          sprintf(buf, "%s looks at you.\r\n", GET_NAME(ch));
          send_to_char(buf, found_char);
        }
        sprintf(buf, "%s looks at %s.\r\n", GET_NAME(ch), GET_NAME(found_char));
        send_to_veh(buf, ch->in_veh, ch, found_char, FALSE);
      }
      ch->in_room = was_in;
      return;
    }
  }
  /* Does the argument match an extra desc in the room? */
  if (ch->in_room && (desc = find_exdesc(arg, ch->in_room->ex_description)) != NULL)
  {
    page_string(ch->desc, desc, 0);
    ch->in_room = was_in;
    return;
  }
  /* Does the argument match a piece of equipment */
  for (j = 0; j < NUM_WEARS && !found; j++)
    if (ch->equipment[j] && CAN_SEE_OBJ(ch, ch->equipment[j]) &&
        isname(arg, ch->equipment[j]->text.keywords))
      if (ch->equipment[j]->text.look_desc)
      {
        send_to_char(ch->equipment[j]->text.look_desc, ch);
        found = TRUE;
      }
  /* Does the argument match an extra desc in the char's equipment? */
  for (j = 0; j < NUM_WEARS && !found; j++)
    if (ch->equipment[j] && CAN_SEE_OBJ(ch, ch->equipment[j]))
      if ((desc = find_exdesc(arg, ch->equipment[j]->ex_description)) != NULL)
      {
        page_string(ch->desc, desc, 1);
        found = 1;
      }
  /* Does the argument match an extra desc in the char's inventory? */
  for (obj = ch->carrying; obj && !found; obj = obj->next_content)
  {
    if (CAN_SEE_OBJ(ch, obj))
      if ((desc = find_exdesc(arg, obj->ex_description)) != NULL) {
        page_string(ch->desc, desc, 1);
        found = 1;
      }
  }
  
  /* Does the argument match an extra desc of an object in the room? */
  if (ch->in_room)
    for (obj = ch->in_room->contents; obj && !found; obj = obj->next_content)
      if (CAN_SEE_OBJ(ch, obj))
        if ((desc = find_exdesc(arg, obj->ex_description)) != NULL)
        {
          send_to_char(desc, ch);
          found = 1;
        }
  if (bits)
  {                   /* If an object was found back in
                       * generic_find */
    if (!found)
      show_obj_to_char(found_obj, ch, 5);       /* Show no-description */
    else
      show_obj_to_char(found_obj, ch, 6);       /* Find hum, glow etc */
  } else if (!found)
    send_to_char(NOOBJECT, ch);
  ch->in_room = was_in;
  
}

ACMD_CONST(do_look) {
  char not_const[MAX_STRING_LENGTH];
  strcpy(not_const, argument);
  ACMD_DECLARE(do_look);
  do_look(ch, not_const, cmd, subcmd);
}

ACMD(do_look)
{
  static char arg2[MAX_INPUT_LENGTH];
  int look_type;
  
  if (!ch->desc)
    return;
  
  if (GET_POS(ch) < POS_SLEEPING)
    send_to_char("You can't see anything but stars!\r\n", ch);
  else if (!LIGHT_OK(ch)) {
    send_to_char("It is pitch black...\r\n", ch);
  } else {
    half_chop(argument, arg, arg2);
    
    if (subcmd == SCMD_READ) {
      if (!*arg)
        send_to_char("Read what?\r\n", ch);
      else
        look_at_target(ch, arg);
      return;
    }
    if (!*arg)                  /* "look" alone, without an argument at all */
      look_at_room(ch, 1);
    else if (is_abbrev(arg, "in"))
      look_in_obj(ch, arg2, FALSE);
    /* did the char type 'look <direction>?' */
    else if ((look_type = search_block(arg, lookdirs, FALSE)) >= 0)
      look_in_direction(ch, convert_look[look_type]);
    else if (is_abbrev(arg, "at"))
      look_at_target(ch, arg2);
    else
      look_at_target(ch, arg);
  }
}

void look_at_veh(struct char_data *ch, struct veh_data *veh, int success)
{
  strcpy(buf, GET_VEH_NAME(veh));
  int cond = 10 - veh->damage;
  if (cond >= 10)
    strcat(buf, " is in perfect condition.\r\n");
  else if (cond >= 9)
    strcat(buf, " has some light scratches.\r\n");
  else if (cond >= 5)
    strcat(buf, " is dented and scratched.\r\n");
  else if (cond >= 3)
    strcat(buf, " has seen better days.\r\n");
  else if (cond >= 1)
    strcat(buf, " is barely holding together.\r\n");
  else
    strcat(buf, " is wrecked.\r\n");
  send_to_char(buf, ch);
  disp_mod(veh, ch, success);
}

void do_probe_veh(struct char_data *ch, struct veh_data * k)
{
  sprintf(buf, "Name: '^y%s^n', Aliases: %s\r\n",
          k->short_description, k->name);
  sprintf(ENDOF(buf), "It is a ^c%s^n with handling ^c%d^n, a top speed of ^c%d^n, and raw acceleration of ^c%d^n.\r\n",
          veh_type[k->type], k->handling, k->speed, k->accel);
  sprintf(ENDOF(buf), "It has a body rating of ^c%d^n and rating-^c%d^n vehicular armor. It seats ^c%d^n up front and ^c%d^n in the back.\r\n",
          k->body, k->armor, k->seating[1], k->seating[0]);
  sprintf(ENDOF(buf), "Its signature rating is ^c%d^n, and its NERP pilot rating is ^c%d^n.\r\n",
          k->sig, k->pilot);
  sprintf(ENDOF(buf), "It has ^c%d^n slots in its autonav and carrying capacity of ^c%d^n (%d in use).\r\n",
          k->autonav, (int)k->load, (int)k->usedload);
  send_to_char(buf, ch);
}

void do_probe_object(struct char_data * ch, struct obj_data * j) {
  int i, found, mount_location;
  bool has_pockets = FALSE, added_extra_carriage_return = FALSE, has_smartlink = FALSE;
  struct obj_data *access = NULL;
  
  if (j->restring) {
    sprintf(buf, "^MOOC^n statistics for '^y%s^n' (restrung from %s):\r\n", GET_OBJ_NAME(j), j->text.name);
  } else {
    sprintf(buf, "^MOOC^n statistics for '^y%s^n':\r\n", (GET_OBJ_NAME(j) ? GET_OBJ_NAME(j) : "<None>"));
  }
  
  sprinttype(GET_OBJ_TYPE(j), item_types, buf1);
  sprintf(ENDOF(buf), "It is %s ^c%s^n that weighs ^c%.2f^n kilos. It is made of ^c%s^n with a durability of ^c%d^n.\r\n",
          AN(buf1), buf1, GET_OBJ_WEIGHT(j), material_names[(int)GET_OBJ_MATERIAL(j)], GET_OBJ_BARRIER(j));
  
  if (strcmp(j->obj_flags.wear_flags.ToString(), "0") != 0) {
    j->obj_flags.wear_flags.PrintBits(buf2, MAX_STRING_LENGTH, wear_bits, NUM_WEARS);
    sprintf(ENDOF(buf), "It can be worn or equipped at the following wear location(s):\r\n  ^c%s^n\r\n", buf2);
  } else {
    sprintf(ENDOF(buf), "This item cannot be worn or equipped.\r\n");
  }
  
  switch (GET_OBJ_TYPE(j))
  {
    case ITEM_LIGHT:
      if (GET_OBJ_VAL(j, 2) == -1) {
        sprintf(ENDOF(buf), "It is an ^cinfinite^n light source.");
      } else {
        sprintf(ENDOF(buf), "It has ^c%d^n hours of light left.", GET_OBJ_VAL(j, 2));
      }
      break;
    case ITEM_FIREWEAPON:
      sprintf(ENDOF(buf), "It is a ^c%s^n that requires ^c%d^n strength to use in combat.\r\n",
              GET_OBJ_VAL(j, 5) == 0 ? "Bow" : "Crossbow", GET_OBJ_VAL(j, 6));
      if (GET_OBJ_VAL(j, 2)) {
        sprintf(ENDOF(buf), "It has a damage code of ^c(STR+%d)%s%s^n", GET_OBJ_VAL(j, 2), wound_arr[GET_OBJ_VAL(j, 1)],
                !IS_DAMTYPE_PHYSICAL(get_weapon_damage_type(j)) ? "( stun)" : "");
      } else {
        sprintf(ENDOF(buf), "It has a damage code of ^c(STR)%s%s^n", wound_arr[GET_OBJ_VAL(j, 1)],
                !IS_DAMTYPE_PHYSICAL(get_weapon_damage_type(j)) ? " (stun)" : "");
      }
      sprintf(ENDOF(buf), "and requires the ^c%s^n skill to use.", skills[GET_OBJ_VAL(j, 4)].name);
      break;
    case ITEM_WEAPON:
      if (IS_GUN(GET_OBJ_VAL((j), 3))) {
        if (GET_OBJ_VAL(j, 5) > 0) {
          sprintf(ENDOF(buf), "It is a ^c%d-round %s^n that uses the ^c%s^n skill to fire. Its damage code is ^c%d%s%s^n.",
                  GET_OBJ_VAL(j, 5), weapon_type[GET_OBJ_VAL(j, 3)], skills[GET_OBJ_VAL(j, 4)].name,
                  GET_OBJ_VAL(j, 0), wound_arr[GET_OBJ_VAL(j, 1)], !IS_DAMTYPE_PHYSICAL(get_weapon_damage_type(j)) ? " (stun)" : "");
        } else {
          sprintf(ENDOF(buf), "It is %s ^c%s^n that uses the ^c%s^n skill to fire. Its damage code is ^c%d%s%s^n.",
                  AN(weapon_type[GET_OBJ_VAL(j, 3)]), weapon_type[GET_OBJ_VAL(j, 3)], skills[GET_OBJ_VAL(j, 4)].name,
                  GET_OBJ_VAL(j, 0), wound_arr[GET_OBJ_VAL(j, 1)], !IS_DAMTYPE_PHYSICAL(get_weapon_damage_type(j)) ? " (stun)" : "");
        }
        
        // Info about attachments, if any.
        for (int i = ACCESS_LOCATION_TOP; i <= ACCESS_LOCATION_UNDER; i++) {
          if (GET_OBJ_VAL(j, i) > 0
              && real_object(GET_OBJ_VAL(j, i)) > 0
              && (access = &obj_proto[real_object(GET_OBJ_VAL(j, i))])) {
            // mount_location: used for gun_accessory_locations[] lookup.
            mount_location = i - ACCESS_LOCATION_TOP;
            
            // format flair
            if (!added_extra_carriage_return) {
              strcat(buf, "\r\n");
              added_extra_carriage_return = TRUE;
            }
            
            // parse and add the string for the accessory's special bonuses
            switch (GET_OBJ_VAL(access, 1)) {
              case ACCESS_SMARTLINK:
                has_smartlink = TRUE;
                sprintf(ENDOF(buf), "\r\nA Smartlink%s attached to the %s provides ^c%d^n to target numbers.",
                        GET_OBJ_VAL(access, 2) < 2 ? "" : "-II", gun_accessory_locations[mount_location],
                        (GET_OBJ_VAL(j, 1) == 1 || GET_OBJ_VAL(access, 2) < 2) ? -2 : -4);
                break;
              case ACCESS_SCOPE:
                if (GET_OBJ_AFFECT(access).IsSet(AFF_LASER_SIGHT)) {
                  sprintf(ENDOF(buf), "\r\nA laser sight attached to the %s provides ^c-1^n to target numbers.",
                          gun_accessory_locations[mount_location]);
                } else {
                  sprintf(ENDOF(buf), "\r\nA scope has been attached to the %s.",
                          gun_accessory_locations[mount_location]);
                }
                break;
              case ACCESS_GASVENT:
                sprintf(ENDOF(buf), "\r\nA gas vent installed in the %s provides ^c%d^n round%s worth of recoil compensation.",
                        gun_accessory_locations[mount_location], -GET_OBJ_VAL(access, 2), -GET_OBJ_VAL(access, 2) > 1 ? "s'" : "'s");
                break;
              case ACCESS_SHOCKPAD:
                sprintf(ENDOF(buf), "\r\nA shock pad installed on the %s provides ^c1^n round's worth of recoil compensation.",
                        gun_accessory_locations[mount_location]);
                break;
              case ACCESS_SILENCER:
                sprintf(ENDOF(buf), "\r\nThe silencer installed on the %s will muffle this weapon's report.",
                        gun_accessory_locations[mount_location]);
                break;
              case ACCESS_SOUNDSUPP:
                sprintf(ENDOF(buf), "\r\nThe suppressor installed on the %s will muffle this weapon's report.",
                        gun_accessory_locations[mount_location]);
                break;
              case ACCESS_SMARTGOGGLE:
                strcat(buf, "\r\n^RYou should never see this-- alert an imm.^n");
                break;
              case ACCESS_BIPOD:
                sprintf(ENDOF(buf), "\r\nA bipod installed on the %s provides ^c%d^n round%s worth of recoil compensation when fired from prone.",
                        gun_accessory_locations[mount_location], RECOIL_COMP_VALUE_BIPOD, RECOIL_COMP_VALUE_BIPOD > 1 ? "s'" : "'s");
                break;
              case ACCESS_TRIPOD:
                sprintf(ENDOF(buf), "\r\nA tripod installed on the %s provides ^c%d^n round%s worth of recoil compensation when fired from prone.",
                        gun_accessory_locations[mount_location], RECOIL_COMP_VALUE_TRIPOD, RECOIL_COMP_VALUE_TRIPOD > 1 ? "s'" : "'s");
                break;
              case ACCESS_BAYONET:
                if (mount_location != ACCESS_LOCATION_UNDER) {
                  strcat(buf, "\r\n^RYour bipod has been mounted in the wrong location and is nonfunctional. Alert an imm.");
                } else {
                  sprintf(ENDOF(buf), "\r\nA bayonet attached to the %s allows you to use the Pole Arms skill when defending from melee attacks.",
                          gun_accessory_locations[mount_location]);
                }
                break;
              default:
                sprintf(buf1, "SYSERR: Unknown accessory type %d passed to do_probe_object()", GET_OBJ_VAL(access, 1));
                log(buf1);
                break;
            }
            // Tack on affect and extra flags to the attachment.
            if (strcmp(GET_OBJ_AFFECT(access).ToString(), "0") != 0) {
              GET_OBJ_AFFECT(access).PrintBits(buf2, MAX_STRING_LENGTH, affected_bits, AFF_MAX);
              sprintf(ENDOF(buf), "\r\n ^- It provides the following flags: ^c%s^n", buf2);
            }
            
            if (strcmp(GET_OBJ_EXTRA(access).ToString(), "0") != 0) {
              GET_OBJ_EXTRA(access).PrintBits(buf2, MAX_STRING_LENGTH, extra_bits, ITEM_EXTRA_MAX);
              sprintf(ENDOF(buf), "\r\n ^- It provides the following extra features: ^c%s^n", buf2);
            }
          }
        }
      } else {
        if (GET_OBJ_VAL(j, 2) > 0) {
          sprintf(ENDOF(buf), "It is a ^c%s^n that uses the ^c%s^n skill to attack with. Its damage code is ^c(STR+%d)%s%s^n.",
                  weapon_type[GET_OBJ_VAL(j, 3)], skills[GET_OBJ_VAL(j, 4)].name, GET_OBJ_VAL(j, 2), wound_arr[GET_OBJ_VAL(j, 1)],
                  !IS_DAMTYPE_PHYSICAL(get_weapon_damage_type(j)) ? " (stun)" : "");
        } else {
          sprintf(ENDOF(buf), "It is a ^c%s^n that uses the ^c%s^n skill to attack with. Its damage code is ^c(STR)%s%s^n.",
                  weapon_type[GET_OBJ_VAL(j, 3)], skills[GET_OBJ_VAL(j, 4)].name, wound_arr[GET_OBJ_VAL(j, 1)],
                  !IS_DAMTYPE_PHYSICAL(get_weapon_damage_type(j)) ? " (stun)" : "");
        }
      }
      break;
    case ITEM_MISSILE:
      if (GET_OBJ_VAL(j, 0) == 0) {
        sprintf(ENDOF(buf), "It is an ^carrow^n suitable for use in a bow.");
      } else {
        sprintf(ENDOF(buf), "It is a ^cbolt^n suitable for use in a crossbow.");
      }
      break;
    case ITEM_WORN:
      sprintf(buf1, "It has space for ");
      
      if (GET_OBJ_VAL(j, 0) > 0) {
        sprintf(ENDOF(buf1), "%s^c%d^n holster%s", (has_pockets?", ":""), GET_OBJ_VAL(j, 0), GET_OBJ_VAL(j, 0) > 1 ? "s":"");
        has_pockets = TRUE;
      }
      if (GET_OBJ_VAL(j, 1) > 0) {
        sprintf(ENDOF(buf1), "%s^c%d^n magazine%s", (has_pockets?", ":""), GET_OBJ_VAL(j, 1), GET_OBJ_VAL(j, 1) > 1 ? "s":"");
        has_pockets = TRUE;
      }
      if (GET_OBJ_VAL(j, 4) > 0) {
        sprintf(ENDOF(buf1), "%s^c%d^n miscellaneous small item%s", (has_pockets?", ":""), GET_OBJ_VAL(j, 4), GET_OBJ_VAL(j, 4) > 1 ? "s":"");
        has_pockets = TRUE;
      }
      
      if (has_pockets) {
        sprintf(ENDOF(buf), "%s.\r\n", buf1);
      } else {
        strcat(buf, "It has no pockets.\r\n");
      }
      sprintf(ENDOF(buf), "It provides ^c%d^n ballistic armor and ^c%d^n impact armor. Its concealability rating is ^c%d^n.",
              GET_OBJ_VAL(j, 5), GET_OBJ_VAL(j, 6), GET_OBJ_VAL(j, 7));
      break;
    case ITEM_DOCWAGON:
      sprintf(ENDOF(buf), "It is a ^c%s^n contract that ^c%s bonded%s^n.",
              docwagon_contract_types[GET_OBJ_VAL(j, 0)], GET_OBJ_VAL(j, 1) ? "is" : "has not been",
              GET_OBJ_VAL(j, 1) ? (GET_OBJ_VAL(j, 1) == GET_IDNUM(ch) ? " to you" : " to someone else") : " to anyone yet");
      break;
    case ITEM_CONTAINER:
      sprintf(ENDOF(buf), "It can hold a maximum of ^c%d^n kilograms.", GET_OBJ_VAL(j, 0));
      break;
    case ITEM_DRINKCON:
    case ITEM_FOUNTAIN:
      sprinttype(GET_OBJ_VAL(j, 2), drinks, buf2);
      sprintf(ENDOF(buf), "It currently contains ^c%d/%d^n units of ^c%s^n.",
              GET_OBJ_VAL(j, 0), GET_OBJ_VAL(j, 1), buf2);
      break;
    case ITEM_MONEY:
      if (!GET_OBJ_VAL(j, 1))
        sprintf(ENDOF(buf), "It is a stack of cash worth ^c%d^n nuyen.", GET_OBJ_VAL(j, 0));
      else
        sprintf(ENDOF(buf), "It is a ^c%s^n-secured ^ccredstick^n loaded with ^c%d^n nuyen.",
                (GET_OBJ_VAL(j, 2) == 1 ? "6-digit PIN" : (GET_OBJ_VAL(j, 2) == 2 ? "thumbprint" : "retina")), GET_OBJ_VAL(j, 0));
      break;
    case ITEM_KEY:
      sprintf(ENDOF(buf), "No OOC information is available about this key.");
      break;
    case ITEM_FOOD:
      sprintf(ENDOF(buf), "It provides ^c%d^n units of nutrition when eaten.", GET_OBJ_VAL(j, 0));
      break;
    case ITEM_QUIVER:
      if (GET_OBJ_VAL(j, 1) >= 0 && GET_OBJ_VAL(j, 1) <= 3) {
        sprintf(ENDOF(buf), "It can hold up to ^c%d^n ^c%s%s^n.", GET_OBJ_VAL(j, 0), projectile_ammo_types[GET_OBJ_VAL(j, 1)],
                GET_OBJ_VAL(j, 0) > 1 ? "s" : "");
      } else {
        sprintf(ENDOF(buf), "It can hold up to ^c%d^n ^cundefined projectiles^n.", GET_OBJ_VAL(j, 0));
      }
      break;
    case ITEM_PATCH:
      sprintf(ENDOF(buf), "It is a ^crating-%d %s^n patch.", GET_OBJ_VAL(j, 1), patch_names[GET_OBJ_VAL(j, 0)]);
      break;
    case ITEM_CYBERDECK:
      sprintf(ENDOF(buf), "MPCP: ^c%d^n, Hardening: ^c%d^n, Active: ^c%d^n, Storage: ^c%d^n, Load: ^c%d^n.",
              GET_OBJ_VAL(j, 0), GET_OBJ_VAL(j, 1), GET_OBJ_VAL(j, 2),
              GET_OBJ_VAL(j, 3), GET_OBJ_VAL(j, 4));
      break;
    case ITEM_PROGRAM:
      sprintf(ENDOF(buf), "It is a ^crating-%d %s^n program, ^c%d^n units in size.",
              GET_OBJ_VAL(j, 1), programs[GET_OBJ_VAL(j, 0)].name, GET_OBJ_VAL(j, 2));
      if (GET_OBJ_VAL(j, 0) == SOFT_ATTACK)
        sprintf(ENDOF(buf), " Its damage code is ^c%s^n.", wound_name[GET_OBJ_VAL(j, 3)]);
      break;
    case ITEM_BIOWARE:
      sprintf(ENDOF(buf), "It is a ^crating-%d %s%s^n that uses ^c%.2f^n index when installed.",
              GET_OBJ_VAL(j, 1), GET_OBJ_VAL(j, 2) || GET_OBJ_VAL(j, 0) >= BIO_CEREBRALBOOSTER ? "cultured " : "",
              decap_bio_types[GET_OBJ_VAL(j, 0)], ((float) GET_OBJ_VAL(j, 4) / 100));
      break;
    case ITEM_CYBERWARE:
      sprintf(ENDOF(buf), "It is a ^crating-%d %s-grade %s^n that uses ^c%.2f^n essence when installed.",
              GET_OBJ_VAL(j, 1), decap_cyber_grades[GET_OBJ_VAL(j, 2)], decap_cyber_types[GET_OBJ_VAL(j, 0)],
              ((float) GET_OBJ_VAL(j, 4) / 100));
      break;
    case ITEM_WORKSHOP:
      sprintf(ENDOF(buf), "It is a ^c%s^n designed for ^c%s^n.",
              GET_OBJ_VAL(j, 1) ? GET_OBJ_VAL(j, 1) == 3 ? "Facility": "Workshop" : "Kit", workshops[GET_OBJ_VAL(j, 0)]);
      break;
    case ITEM_FOCUS:
      sprintf(ENDOF(buf), "It is a ^c%s^n focus of force ^c%d^n.", foci_type[GET_OBJ_VAL(j, 0)], GET_OBJ_VAL(j, 1));
      break;
    case ITEM_SPELL_FORMULA:
      sprintf(ENDOF(buf), "It is a ^cforce-%d %s^n designed for ^c%s^n mages.", GET_OBJ_VAL(j, 0),
              spells[GET_OBJ_VAL(j, 1)].name, GET_OBJ_VAL(j, 2) ? "Shamanic" : "Hermetic");
      break;
    case ITEM_PART:
      sprintf(ENDOF(buf), "It is %s ^c%s^n designed for MPCP ^c%d^n decks.", AN(parts[GET_OBJ_VAL(j, 0)].name),
              parts[GET_OBJ_VAL(j, 0)].name, GET_OBJ_VAL(j, 2));
      break;
    case ITEM_CUSTOM_DECK:
      sprintf(ENDOF(buf), "You should EXAMINE this deck, or jack in and view its SOFTWARE.");
      break;
    case ITEM_DRUG:
      sprintf(ENDOF(buf), "It is a dose of ^c%s^n.", drug_types[GET_OBJ_VAL(j, 0)].name);
      break;
    case ITEM_MAGIC_TOOL:
      sprintf(ENDOF(buf), "It is a ^crating-^c%d %s^n.", GET_OBJ_VAL(j, 1), magic_tool_types[GET_OBJ_VAL(j, 0)]);
      break;
    case ITEM_RADIO:
      sprintf(ENDOF(buf), "It has a ^c%d/5^n range and can encrypt and decrypt signals up to crypt level ^c%d^n.",
              GET_OBJ_VAL(j, 1), GET_OBJ_VAL(j, 2));
      break;
    case ITEM_GUN_ACCESSORY:
      if (GET_OBJ_VAL(j, 1) == ACCESS_SMARTLINK || GET_OBJ_VAL(j, 1) == ACCESS_GASVENT) {
        sprintf(ENDOF(buf), "It is a ^crating-%d %s^n that attaches to the ^c%s^n.",
                GET_OBJ_VAL(j, 2), gun_accessory_types[GET_OBJ_VAL(j, 1)], gun_accessory_locations[GET_OBJ_VAL(j, 0)]);
      } else {
        sprintf(ENDOF(buf), "It is %s ^c%s^n that attaches to the ^c%s^n.",
                AN(gun_accessory_types[GET_OBJ_VAL(j, 1)]), gun_accessory_types[GET_OBJ_VAL(j, 1)]
                , gun_accessory_locations[GET_OBJ_VAL(j, 0)]);
      }
      
      break;
    case ITEM_GYRO:
    case ITEM_CLIMBING:
    case ITEM_RCDECK:
      sprintf(ENDOF(buf), "Its rating is ^c%d^n.", GET_OBJ_VAL(j, 0));
      break;
    case ITEM_CHIP:
      sprintf(ENDOF(buf), "It grants the skill ^c%s^n at rating ^c%d^n.", skills[GET_OBJ_VAL(j, 0)].name, GET_OBJ_VAL(j, 1));
      break;
    case ITEM_HOLSTER:
      sprintf(ENDOF(buf), "It is designed for a ^c%s^n.", holster_types[GET_OBJ_VAL(j, 0)]);
      break;
    case ITEM_DECK_ACCESSORY:
      if (GET_OBJ_VAL(j, 0) == TYPE_FILE) {
        sprintf(ENDOF(buf), "This file requires ^c%d^n units of space.", GET_OBJ_VAL(j, 2));
      } else if (GET_OBJ_VAL(j, 0) == TYPE_UPGRADE) {
        if (GET_OBJ_VAL(j, 1) == 3) {
          sprintf(ENDOF(buf), "This cyberdeck upgrade affects ^c%s^n with a rating of ^c%d^n.",
                  deck_accessory_upgrade_types[GET_OBJ_VAL(j, 1)], GET_OBJ_VAL(j, 2));
          if (GET_OBJ_VAL(j, 1) == 5) {
            sprintf(ENDOF(buf), "\r\nIt is designed for MPCP rating ^c%d^n.", GET_OBJ_VAL(j, 3));
          }
        } else {
          strcat(buf, "This cyberdeck upgrade adds a ^chitcher jack^n.");
        }
      } else if (GET_OBJ_VAL(j, 0) == TYPE_COMPUTER) {
        sprintf(ENDOF(buf), "This computer has ^c%d^n units of active memory and ^c%d^n units of storage memory.",
                GET_OBJ_VAL(j, 1), GET_OBJ_VAL(j, 2));
      } else if (GET_OBJ_VAL(j, 0) == TYPE_PARTS) {
        sprintf(ENDOF(buf), "This pack of parts contains ^c%d^n units of ^c%s^n.",
                GET_OBJ_COST(j), GET_OBJ_VAL(j, 1) == 0 ? "general parts" : "memory chips");
      } else {
        sprintf(buf2, "Error: Unknown ITEM_DECK_ACCESSORY type %d passed to probe command.", GET_OBJ_VAL(j, 0));
        log(buf2);
      }
      break;
    case ITEM_MOD:
      sprintf(ENDOF(buf), "It is %s ^c%s^n upgrade", AN(mod_types[GET_OBJ_VAL(j, 0)].name), mod_types[GET_OBJ_VAL(j, 0)].name);
      
      // Val 1
      if (GET_OBJ_VAL(j, 0) == TYPE_MOUNT) {
        sprintf(ENDOF(buf), " that adds %s ^c%s^n.", AN(mount_types[GET_OBJ_VAL(j, 1)]), mount_types[GET_OBJ_VAL(j, 1)]);
      } else {
        sprintf(ENDOF(buf), " that takes up ^c%d^n load space.", GET_OBJ_VAL(j, 1));
      }
      
      // Val 2
      if (GET_OBJ_VAL(j, 0) == MOD_ENGINE) {
        // engine type
        sprintf(ENDOF(buf), "\r\nIt is %s ^c%s^n engine.", AN(engine_type[GET_OBJ_VAL(j, 2)]), engine_type[GET_OBJ_VAL(j, 2)]);
      } else if (GET_OBJ_VAL(j, 0) == MOD_RADIO) {
        // radio range 0-5
        sprintf(ENDOF(buf), "\r\nIt has a ^c%d/5^n range and can encrypt and decrypt signals up to crypt level ^c%d^n.",
                GET_OBJ_VAL(j, 2), GET_OBJ_VAL(j, 3));
      } else {
        sprintf(ENDOF(buf), "\r\nIt functions at rating ^c%d^n.", GET_OBJ_VAL(j, 2));
      }
      
      // Val 5
      sprintbit(GET_OBJ_VAL(j, 5), engine_type, buf2);
      sprintf(ENDOF(buf), "\r\nIt is compatible with the following engine types:\r\n^c  %s^n", buf2);
      
      // Vals 4 and 6
      sprintf(ENDOF(buf), "\r\nIt has been designed to fit ^c%s^n, and installs to the ^c%s^n.",
              GET_OBJ_VAL(j, 4) == 0 ? "vehicles" : GET_OBJ_VAL(j, 4) == 1 ? "drones" : "all types of vehicles",
              mod_name[GET_OBJ_VAL(j, 6)]);
      break;
    case ITEM_DESIGN:
      if (GET_OBJ_VAL(j, 0) == 5) {
        sprintf(ENDOF(buf), "This design is for a ^crating-%d %s (%s)^n program. It requires ^c%d^n units of storage.\r\n",
                GET_OBJ_VAL(j, 1), programs[GET_OBJ_VAL(j, 0)].name, wound_name[GET_OBJ_VAL(j, 2)],
                (GET_OBJ_VAL(j, 1) * GET_OBJ_VAL(j, 1)) * attack_multiplier[GET_OBJ_VAL(j, 2)]);
      } else {
        sprintf(ENDOF(buf), "This design is for a ^crating-%d %s^n program. It requires ^c%d^n units of storage.\r\n",
                GET_OBJ_VAL(j, 1), programs[GET_OBJ_VAL(j, 0)].name,
                (GET_OBJ_VAL(j, 1) * GET_OBJ_VAL(j, 1)) * programs[GET_OBJ_VAL(j, 0)].multiplier);
      }
      break;
    case ITEM_GUN_AMMO:
      if (GET_OBJ_VAL(j, 3))
        send_to_char(ch, "It has %d/%d %s round%s of %s ammunition left.\r\n", GET_OBJ_VAL(j, 0), GET_OBJ_VAL(j, 0) +
                     GET_OBJ_VAL(j, 3), ammo_type[GET_OBJ_VAL(j, 2)].name,GET_OBJ_VAL(j, 0) != 1 ? "s" : "",
                     weapon_type[GET_OBJ_VAL(j, 1)]);
      else
        send_to_char(ch, "It has %d %s round%s of %s ammunition left.\r\n", GET_OBJ_VAL(j, 0),
                     ammo_type[GET_OBJ_VAL(j, 2)].name,GET_OBJ_VAL(j, 0) != 1 ? "s" : "",
                     weapon_type[GET_OBJ_VAL(j, 1)]);
      break;
    case ITEM_GUN_MAGAZINE:
      // All info about these is displayed when you examine them.
    case ITEM_QUEST:
    case ITEM_OTHER:
    case ITEM_CAMERA:
    case ITEM_PHONE:
      sprintf(ENDOF(buf), "Nothing stands out about this item's OOC values.");
      break;
    default:
      strcat(buf, "This item type has no probe string. Contact the staff to request one.");
      break;
  }
  
  if (GET_OBJ_AFFECT(j).IsSet(AFF_LASER_SIGHT) && has_smartlink) {
    sprintf(ENDOF(buf), "\r\n\r\n^yWARNING:^n Your smartlink overrides your laser sight-- the laser will not function.");
  }
  
  strcat(buf, "^n\r\n\r\n");
  found = 0;
  
  if (strcmp(j->obj_flags.bitvector.ToString(), "0") != 0) {
    j->obj_flags.bitvector.PrintBits(buf2, MAX_STRING_LENGTH, affected_bits, AFF_MAX);
    sprintf(ENDOF(buf), "This object provides the following flags when used: ^c%s^n\r\n", buf2);
  }
  
  if (strcmp(GET_OBJ_EXTRA(j).ToString(), "0") != 0) {
    GET_OBJ_EXTRA(j).PrintBits(buf2, MAX_STRING_LENGTH, extra_bits, ITEM_EXTRA_MAX);
    sprintf(ENDOF(buf), "This object has the following extra features: ^c%s^n\r\n", buf2);
  }
  
  sprintf(buf1, "This object modifies your character in the following ways when used:\r\n  ^c");
  for (i = 0; i < MAX_OBJ_AFFECT; i++)
    if (j->affected[i].modifier)
    {
      if (GET_OBJ_TYPE(j) == ITEM_MOD)
        sprinttype(j->affected[i].location, veh_aff, buf2);
      else
        sprinttype(j->affected[i].location, apply_types, buf2);
      sprintf(ENDOF(buf1), "%s %+d to %s", found++ ? "," : "",
              j->affected[i].modifier, buf2);
    }
  if (found) {
    strcat(buf, buf1);
    strcat(buf, "^n\r\n");
  }
  
  if (j->source_info) {
    sprintf(ENDOF(buf), "\r\nIt references data from the following source book(s): %s^n\r\n", j->source_info);
  }
  
  send_to_char(buf, ch);
}

ACMD(do_examine)
{
  int i, skill = 0;
  struct char_data *tmp_char;
  struct obj_data *tmp_object;
  struct veh_data *found_veh;
  one_argument(argument, arg);
  
  if (!*arg) {
    send_to_char("Examine what?\r\n", ch);
    return;
  }
  if (subcmd == SCMD_EXAMINE)
    look_at_target(ch, arg);
  
  if (!ch->in_veh || (ch->in_veh && !ch->vfront)) {
    found_veh = get_veh_list(arg, ch->in_veh ? ch->in_veh->carriedvehs : ch->in_room->vehicles, ch);
    if (found_veh) {
      if (subcmd == SCMD_PROBE) {
        // If they don't own the vehicle and the hood isn't open, they can't view the stats.
        if (GET_IDNUM(ch) != found_veh->owner && !found_veh->hood) {
          send_to_char("You can only see the OOC stats for vehicles you own or vehicles that have popped hoods.\r\n", ch);
          return;
        }
        
        // Display the vehicle's info.
        do_probe_veh(ch, found_veh);
        return;
      }
      
      switch(found_veh->type) {
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
      int target = (found_veh->cspeed > SPEED_IDLE) ? 8 : 4;
      skill = get_skill(ch, skill, target);
      target += modify_target(ch);
      if (GET_IDNUM(ch) == found_veh->owner || found_veh->hood)
        i = 12;
      else
        i = success_test(skill + GET_POWER(ch, ADEPT_IMPROVED_PERCEPT), target);
      look_at_veh(ch, found_veh, i);
      return;
    }
  }
  
  if ((!str_cmp(arg, "self") || !str_cmp(arg, "me") || !str_cmp(arg, "myself"))) {
    struct veh_data *target_veh = NULL;
    if (AFF_FLAGGED(ch, AFF_RIG))
      target_veh = ch->in_veh;
    else if (PLR_FLAGGED(ch, PLR_REMOTE))
      target_veh = ch->char_specials.rigging;
    
    if (target_veh) {
      if (subcmd == SCMD_PROBE) {
        do_probe_veh(ch, target_veh);
      } else {
        look_at_veh(ch, target_veh, 12);
      }
      return;
    }
  }
  
  if (subcmd == SCMD_PROBE) {
    generic_find(arg, FIND_OBJ_INV | FIND_OBJ_EQUIP, ch, &tmp_char, &tmp_object);
    
    if (tmp_object) {
      do_probe_object(ch, tmp_object);
    } else {
      send_to_char("You're not carrying any such object, and there are no vehicles like that here.\r\n", ch);
    }
    return;
  } else {
    generic_find(arg, FIND_OBJ_INV | FIND_OBJ_ROOM | FIND_CHAR_ROOM |
                 FIND_OBJ_EQUIP, ch, &tmp_char, &tmp_object);
  }
  
  if (tmp_object) {
    if (GET_OBJ_TYPE(tmp_object) == ITEM_CONTAINER ||
        (GET_OBJ_TYPE(tmp_object) == ITEM_WORN && tmp_object->contains)) {
      if (!tmp_object->contains)
        send_to_char("Looking inside reveals it to be empty.\r\n", ch);
      else {
        struct obj_data *exobj = NULL;
        if (GET_OBJ_VNUM(tmp_object) > 0)
          exobj = read_object(GET_OBJ_RNUM(tmp_object), REAL);
        if (exobj) {
          int q = GET_OBJ_VAL(tmp_object, 0) - (int)(GET_OBJ_WEIGHT(exobj)), x = (int)(GET_OBJ_WEIGHT(tmp_object) - GET_OBJ_WEIGHT(exobj));
          extract_obj(exobj);
          if (x >= q * .9)
            send_to_char("It is bursting at the seams.\r\n", ch);
          else if (x >= q / 2)
            send_to_char("It is more than half full.\r\n", ch);
          else send_to_char("It is less than half full.\r\n", ch);
        }
        send_to_char("When you look inside, you see:\r\n", ch);
        look_in_obj(ch, arg, TRUE);
      }
    } else if ((GET_OBJ_TYPE(tmp_object) == ITEM_WEAPON) &&
               (IS_GUN(GET_OBJ_VAL(tmp_object, 3)))) {
      for (i = 7; i < 10; ++i)
        if (GET_OBJ_VAL(tmp_object, i) >= 0) {
          sprintf(buf1, "There is %s attached to the %s of it.\r\n",
                  GET_OBJ_VAL(tmp_object, i) > 0 ? short_object(GET_OBJ_VAL(tmp_object, i), 2) : "nothing",
                  (i == 7 ? "top" : (i == 8 ? "barrel" : "bottom")));
          send_to_char(buf1, ch);
        }
      if (tmp_object->contains)
        sprintf(buf, "It contains %d round%s of %s ammunition, and can hold %d round%s.\r\n",
                MIN(GET_OBJ_VAL(tmp_object->contains, 9), GET_OBJ_VAL(tmp_object, 5)),
                (GET_OBJ_VAL(tmp_object->contains, 9) != 1 ? "s" : ""), ammo_type[GET_OBJ_VAL(tmp_object->contains, 2)].name,
                GET_OBJ_VAL(tmp_object, 5), (GET_OBJ_VAL(tmp_object, 5) != 1 ? "s" : ""));
      else
        sprintf(buf, "It does not contain any ammunition, but looks to hold %d round%s.\r\n",
                GET_OBJ_VAL(tmp_object, 5), (GET_OBJ_VAL(tmp_object, 5) != 1 ? "s" : ""));
      if (GET_OBJ_VAL(tmp_object, 5) > 0)
        send_to_char(buf, ch);
    } else if (GET_OBJ_TYPE(tmp_object) == ITEM_MONEY) {
      if (!GET_OBJ_VAL(tmp_object, 1))    // paper money
        send_to_char(ch, "There looks to be about %d nuyen.\r\n", GET_OBJ_VAL(tmp_object, 0));
      else {
        if (GET_OBJ_VAL(tmp_object, 4)) {
          if (belongs_to(ch, tmp_object))
            send_to_char("It has been activated by you.\r\n", ch);
          else
            send_to_char("It has been activated.\r\n", ch);
        } else
          send_to_char("It has not been activated.\r\n", ch);
        send_to_char(ch, "The account display shows %d nuyen.\r\n", GET_OBJ_VAL(tmp_object, 0));
      }
    } else if (GET_OBJ_TYPE(tmp_object) == ITEM_GUN_MAGAZINE) {
      send_to_char(ch, "It has %d %s round%s left.\r\n", GET_OBJ_VAL(tmp_object, 9),
                   ammo_type[GET_OBJ_VAL(tmp_object, 2)].name,GET_OBJ_VAL(tmp_object, 9) != 1 ? "s" : "");
      send_to_char(ch, "It can hold a maximum of %d %s round%s.\r\n", GET_OBJ_VAL(tmp_object, 0), weapon_type[GET_OBJ_VAL(tmp_object, 1)],
                   GET_OBJ_VAL(tmp_object, 0) != 1 ? "s" : "");
    } else if (GET_OBJ_TYPE(tmp_object) == ITEM_GUN_AMMO) {
      if (GET_OBJ_VAL(tmp_object, 3))
        send_to_char(ch, "It has %d/%d %s round%s of %s ammunition left.\r\n", GET_OBJ_VAL(tmp_object, 0), GET_OBJ_VAL(tmp_object, 0) +
                     GET_OBJ_VAL(tmp_object, 3), ammo_type[GET_OBJ_VAL(tmp_object, 2)].name,GET_OBJ_VAL(tmp_object, 0) != 1 ? "s" : "",
                     weapon_type[GET_OBJ_VAL(tmp_object, 1)]);
      else
        send_to_char(ch, "It has %d %s round%s of %s ammunition left.\r\n", GET_OBJ_VAL(tmp_object, 0),
                     ammo_type[GET_OBJ_VAL(tmp_object, 2)].name,GET_OBJ_VAL(tmp_object, 0) != 1 ? "s" : "",
                     weapon_type[GET_OBJ_VAL(tmp_object, 1)]);
    } else if (GET_OBJ_VNUM(tmp_object) == 119 && GET_OBJ_VAL(tmp_object, 0) == GET_IDNUM(ch))
      sprintf(buf, "The card contains %d nuyen.\r\n", GET_OBJ_VAL(tmp_object, 1));
    else if (GET_OBJ_TYPE(tmp_object) == ITEM_DECK_ACCESSORY && GET_OBJ_VAL(tmp_object, 0) == TYPE_COOKER) {
      if (!tmp_object->contains)
        sprintf(buf, "The small LED is currently off.\r\n");
      else if (GET_OBJ_VAL(tmp_object, 9))
        sprintf(buf, "The small LED is currently orange, indicating activity.\r\n");
      else if (GET_OBJ_TIMER(tmp_object->contains) == -1)
        sprintf(buf, "The small LED is currently flashed red, indicating a failed encode.\r\n");
      else
        sprintf(buf, "The small LED is currently green, indicating a successful encode.\r\n");
      send_to_char(buf, ch);
    } else if (GET_OBJ_TYPE(tmp_object) == ITEM_PROGRAM && (GET_OBJ_VAL(tmp_object, 0) >= SOFT_ASIST_COLD || GET_OBJ_VAL(tmp_object, 0) < SOFT_SENSOR)) {
      if (GET_OBJ_TIMER(tmp_object) < 0)
        sprintf(buf, "This chip has been ruined.\r\n");
      else if (!GET_OBJ_TIMER(tmp_object))
        sprintf(buf, "This program still needs to be encoded to a chip.\r\n");
      else if (GET_OBJ_TIMER(tmp_object) > 0)
        sprintf(buf, "This program has been encoded onto an optical chip.\r\n");
      send_to_char(buf, ch);
    } else if (GET_OBJ_TYPE(tmp_object) == ITEM_DECK_ACCESSORY && GET_OBJ_VAL(tmp_object, 0) == TYPE_PARTS)
      send_to_char(ch, "There seems to be about %d nuyen worth of %s there.\r\n", GET_OBJ_COST(tmp_object), GET_OBJ_VAL(tmp_object, 1) ? "chips" : "parts");
    else if (GET_OBJ_TYPE(tmp_object) == ITEM_CUSTOM_DECK) {
      if (!GET_OBJ_VAL(tmp_object, 9) && GET_OBJ_VAL(tmp_object, 0) > 0) {
        send_to_char(ch, "This deck is functional.\r\n");
      } else {
        int target = 4;
        int skill = get_skill(ch, SKILL_BR_COMPUTER, target);
        if (success_test(skill, target) > 0) {
          strcpy(buf, "Custom Components:\r\n");
          for (struct obj_data *soft = tmp_object->contains; soft; soft = soft->next_content)
            if (GET_OBJ_TYPE(soft) == ITEM_PART)
              sprintf(ENDOF(buf), "%-30s Type: %-15s Rating: %d\r\n",
                      GET_OBJ_NAME(soft),
                      parts[GET_OBJ_VAL(soft, 0)].name,
                      GET_PART_RATING(soft));
          send_to_char(buf, ch);
        }
      }
    } else if (GET_OBJ_TYPE(tmp_object) == ITEM_MAGIC_TOOL) {
      switch (GET_OBJ_VAL(tmp_object, 0)) {
        case TYPE_CIRCLE:
          send_to_char(ch, "It has been dedicated to %s.\r\n", elements[GET_OBJ_VAL(tmp_object, 2)].name);
          if (GET_OBJ_VAL(tmp_object, 9) && GET_OBJ_VAL(tmp_object, 3) == GET_IDNUM(ch))
            send_to_char(ch, "It is about %d%% completed.\r\n", (int)(((float)((GET_OBJ_VAL(tmp_object, 1) * 60) -
                                                                               GET_OBJ_VAL(tmp_object, 9)) / (float)((GET_OBJ_VAL(tmp_object, 1) != 0 ? GET_OBJ_VAL(tmp_object, 1) : 1) * 60)) * 100));
          
          break;
        case TYPE_LODGE:
          send_to_char(ch, "It has been dedicated to %s.\r\n", totem_types[GET_OBJ_VAL(tmp_object, 2)]);
          if (GET_OBJ_VAL(tmp_object, 9) && GET_OBJ_VAL(tmp_object, 3) == GET_IDNUM(ch))
            send_to_char(ch, "It is about %d%% completed.\r\n", (int)(((float)((GET_OBJ_VAL(tmp_object, 1) * 300) -
                                                                               GET_OBJ_VAL(tmp_object, 9)) / (float)((GET_OBJ_VAL(tmp_object, 1) != 0 ? GET_OBJ_VAL(tmp_object, 1) : 1) * 300)) * 100));
          break;
        case TYPE_SUMMONING:
          send_to_char(ch, "There seems to be about %d nuyen worth.\r\n", GET_OBJ_COST(tmp_object));
          break;
        case TYPE_LIBRARY_CONJURE:
        case TYPE_LIBRARY_SPELL:
          send_to_char(ch, "It seems to have enough material to be classified as rating %d.\r\n", GET_OBJ_VAL(tmp_object, 1));
          break;
      }
    } else if (GET_OBJ_TYPE(tmp_object) == ITEM_SPELL_FORMULA) {
      if (GET_OBJ_TIMER(tmp_object) < 0) {
        if (GET_OBJ_VAL(tmp_object, 8) == GET_IDNUM(ch)) {
          int timeleft = GET_OBJ_VAL(tmp_object, 6);
          if (GET_OBJ_TIMER(tmp_object) == -3)
            timeleft *= 2;
          if (GET_OBJ_VAL(tmp_object, 6))
            send_to_char(ch, "You are about %d%% done.\r\n", (int)(((float)timeleft / (float)GET_OBJ_VAL(tmp_object, 7)) * -100 + 100));
          else send_to_char("You haven't started designing this spell yet.\r\n", ch);
        } else send_to_char(ch, "It doesn't seem to be completed.\r\n");
      } else if (GET_SKILL(ch, SKILL_SORCERY)) {
        send_to_char(ch, "It is a rating %d spell formula describing %s. It is designed for use by %s mages.\r\n",
                     GET_OBJ_VAL(tmp_object, 0), spells[GET_OBJ_VAL(tmp_object, 1)].name, GET_OBJ_VAL(tmp_object, 2) == 1 ?
                     "shamanic" : "hermetic");
      }
    } else if (GET_OBJ_TYPE(tmp_object) == ITEM_DOCWAGON) {
      if (GET_DOCWAGON_BONDED_IDNUM(tmp_object)) {
        if (GET_DOCWAGON_BONDED_IDNUM(tmp_object) == GET_IDNUM(ch))
          send_to_char(ch, "It has been bonded to your biometric signature%s.\r\n\r\n", tmp_object->worn_by != ch ? ", but ^yit won't function until you wear it^n" : "");
        else
          send_to_char("It has been activated by someone else and ^ywill not function for you^n.\r\n\r\n", ch);
      } else {
        send_to_char("It has not been BONDed yet, and ^ywill not function until it is^n.\r\n\r\n", ch);
      }
    }
    if (GET_OBJ_VNUM(tmp_object) > 1) {
      sprintf(buf, "You think that %s ", GET_OBJ_NAME(tmp_object));
      int targ = GET_LEGAL_NUM(tmp_object);
      if (targ <= 0)
        strcat(buf, "is completely legal");
      else {
        int skill = get_skill(ch, SKILL_POLICE_PROCEDURES, targ), suc = success_test(skill, targ);
        if (suc <= 0)
          strcat(buf, "is completely legal");
        else {
          if (GET_LEGAL_NUM(tmp_object) <= 3)
            strcat(buf, "is highly illegal");
          else if (GET_LEGAL_NUM(tmp_object) <= 6)
            strcat(buf, "is illegal");
          else strcat(buf, "could be considered illegal");
        }
      }
      send_to_char(ch, "%s.\r\n", buf);
    }
    get_obj_condition(ch, tmp_object);
  }
}

ACMD(do_gold)
{
  if (GET_NUYEN(ch) == 0)
    send_to_char("You're broke!\r\n", ch);
  else if (GET_NUYEN(ch) == 1)
    send_to_char("You have one miserable nuyen.\r\n", ch);
  else {
    sprintf(buf, "You have %ld nuyen.\r\n", GET_NUYEN(ch));
    send_to_char(buf, ch);
  }
}

ACMD(do_pool)
{
  char pools[MAX_INPUT_LENGTH];
  if (GET_POWER(ch, ADEPT_SIDESTEP) && GET_DEFENSE(ch))
    sprintf(buf, "^R+%d^n", GET_POWER(ch, ADEPT_SIDESTEP));
  else
    sprintf(buf, "  ");
  
  if (PRF_FLAGGED(ch, PRF_SCREENREADER)) {
    sprintf(pools, "  Dodge: %d%s\r\n  Body: %d\r\n  Offense: %d\r\n  Total Combat Dice: %d\r\n",
            GET_DEFENSE(ch), buf, GET_BODY(ch), GET_OFFENSE(ch), GET_COMBAT(ch));
  } else {
    sprintf(pools, "  Combat: %d     (Dodge: %d%s      Body: %d     Offense: %d)\r\n",
            GET_COMBAT(ch), GET_DEFENSE(ch), buf, GET_BODY(ch), GET_OFFENSE(ch));
  }
  if (GET_ASTRAL(ch) > 0)
    sprintf(ENDOF(pools), "  Astral: %d\r\n", GET_ASTRAL(ch));
  if (GET_HACKING(ch) > 0)
    sprintf(ENDOF(pools), "  Hacking: %d\r\n", GET_HACKING(ch));
  if (GET_MAGIC(ch) > 0) {
    if (PRF_FLAGGED(ch, PRF_SCREENREADER)) {
      sprintf(ENDOF(pools), "  Spell: %d\r\n  Casting: %d\r\n  Drain: %d\r\n  Defense: %d\r\n", GET_MAGIC(ch), GET_CASTING(ch), GET_DRAIN(ch), GET_SDEFENSE(ch));
      if (GET_METAMAGIC(ch, META_REFLECTING) == 2)
        sprintf(ENDOF(pools), "  Reflecting: %d\r\n", GET_REFLECT(ch));
    } else {
      sprintf(ENDOF(pools), "  Spell: %d      (Casting: %d    Drain: %d    Defense: %d", GET_MAGIC(ch), GET_CASTING(ch), GET_DRAIN(ch), GET_SDEFENSE(ch));
      if (GET_METAMAGIC(ch, META_REFLECTING) == 2)
        sprintf(ENDOF(pools), "    Reflecting: %d", GET_REFLECT(ch));
      strcat(pools, ")\r\n");
    }
  }
  if (GET_CONTROL(ch) > 0)
    sprintf(ENDOF(pools), "  Control: %d\r\n", GET_CONTROL(ch));
  for (int x = 0; x < 7; x++)
    if (GET_TASK_POOL(ch, x) > 0)
      sprintf(ENDOF(pools), "  %s Task Pool: %d\r\n", attributes[x], GET_TASK_POOL(ch, x));
  send_to_char(pools, ch);
}

const char *get_position_string(struct char_data *ch) {
  static char position_string[200];
  
  if (AFF_FLAGGED(ch, AFF_PRONE))
    strcpy(position_string, "laying prone.");
  else switch (GET_POS(ch)) {
    case POS_DEAD:
      strcpy(position_string, "DEAD!");
      break;
    case POS_MORTALLYW:
      strcpy(position_string, "mortally wounded!  You should seek help!");
      break;
    case POS_STUNNED:
      strcpy(position_string, "stunned!  You can't move!");
      break;
    case POS_SLEEPING:
      strcpy(position_string, "sleeping.");
      break;
    case POS_RESTING:
      strcpy(position_string, "resting.");
      break;
    case POS_SITTING:
      strcpy(position_string, "sitting.");
      break;
    case POS_FIGHTING:
      if (FIGHTING(ch))
        sprintf(position_string, "fighting %s.",PERS(FIGHTING(ch), ch));
      else if (FIGHTING_VEH(ch))
        sprintf(position_string, "fighting %s.", GET_VEH_NAME(FIGHTING_VEH(ch)));
      else
        strcpy(position_string, "fighting thin air.");
      break;
    case POS_STANDING:
      if (IS_WATER(ch->in_room))
        strcpy(position_string, "swimming.");
      else
        strcpy(position_string, "standing.");
      break;
    case POS_LYING:
      strcpy(position_string, "lying down.");
      break;
    default:
      strcpy(position_string, "floating.");
      break;
  }
  
  return position_string;
}

const char *get_vision_string(struct char_data *ch, bool ascii_friendly=FALSE) {
  if (PLR_FLAGGED(ch, PLR_PERCEIVE) || IS_PROJECT(ch)) {
    if (ascii_friendly)
      return "You are astrally perceiving.";
    else
      return "You are astrally perceiving.\r\n";
  }
  
  if (ascii_friendly) {
    if (AFF_FLAGGED(ch, AFF_DETECT_INVIS) && get_ch_in_room(ch)->silence[0] <= 0)
        return "You have ultrasonic vision.";
  } else {
    if (AFF_FLAGGED(ch, AFF_DETECT_INVIS)) {
      if (get_ch_in_room(ch)->silence[0] > 0)
        return "Your ultrasonic vision is being suppressed by a field of silence here.\r\n";
      else
        return "You have ultrasonic vision.\r\n";
    }
  }
  
  if (CURRENT_VISION(ch) == THERMOGRAPHIC) {
    if (ascii_friendly)
      return "You have thermographic vision.";
    else
      return "You have thermographic vision.\r\n";
  }
  
  if (CURRENT_VISION(ch) == LOWLIGHT) {
    if (ascii_friendly)
      return "You have low-light vision.";
    else
      return "You have low-light vision.\r\n";
  }
  
  return "";
}

const char *get_plaintext_score_health(struct char_data *ch) {
  int mental = GET_MENTAL(ch), physical = GET_PHYSICAL(ch);
  bool pain_editor = FALSE;
  for (struct obj_data *bio = ch->bioware; bio; bio = bio->next_content)
    if (GET_OBJ_VAL(bio, 0) == BIO_DAMAGECOMPENSATOR) {
      mental += GET_OBJ_VAL(bio, 1) * 100;
      physical += GET_OBJ_VAL(bio, 1) * 100;
    } else if (GET_OBJ_VAL(bio, 0) == BIO_PAINEDITOR && GET_OBJ_VAL(bio, 3)) {
      pain_editor = TRUE;
      mental = 1000;
      physical = 1000;
      break;
    }
  
  if (pain_editor)
    return "Your physical and mental status is masked by your pain editor.\r\n";
  
  sprintf(buf2, "Mental condition: %d / %d\r\n", (int)(mental / 100), (int)(GET_MAX_MENTAL(ch) / 100));
  
  sprintf(ENDOF(buf2), "Physical condition: %d / %d\r\n", MAX((int)(physical / 100), 0), (int)(GET_MAX_PHYSICAL(ch) / 100));
  
  if (physical < 0)
    sprintf(ENDOF(buf2), "Physical damage overflow: %d\r\n", (int)(physical / 100) * -1);
  
  return buf2;
}

const char *get_plaintext_score_stats(struct char_data *ch) {
  if (GET_BOD(ch) != GET_REAL_BOD(ch))
    sprintf(buf2, "Body: %d (base body %d)\r\n", GET_BOD(ch), GET_REAL_BOD(ch));
  else
    sprintf(buf2, "Body: %d\r\n", GET_BOD(ch));
  
  if (GET_QUI(ch) != GET_REAL_QUI(ch))
    sprintf(ENDOF(buf2), "Quickness: %d (base quickness %d)\r\n", GET_QUI(ch), GET_REAL_QUI(ch));
  else
    sprintf(ENDOF(buf2), "Quickness: %d\r\n", GET_QUI(ch));
  
  if (GET_STR(ch) != GET_REAL_STR(ch))
    sprintf(ENDOF(buf2), "Strength: %d (base strength %d)\r\n", GET_STR(ch), GET_REAL_STR(ch));
  else
    sprintf(ENDOF(buf2), "Strength: %d\r\n", GET_STR(ch));
  
  if (GET_CHA(ch) != GET_REAL_CHA(ch))
    sprintf(ENDOF(buf2), "Charisma: %d (base charisma %d)\r\n", GET_CHA(ch), GET_REAL_CHA(ch));
  else
    sprintf(ENDOF(buf2), "Charisma: %d\r\n", GET_CHA(ch));
  
  if (GET_INT(ch) != GET_REAL_INT(ch))
    sprintf(ENDOF(buf2), "Intelligence: %d (base intelligence %d)\r\n", GET_INT(ch), GET_REAL_INT(ch));
  else
    sprintf(ENDOF(buf2), "Intelligence: %d\r\n", GET_INT(ch));
  
  if (GET_WIL(ch) != GET_REAL_WIL(ch))
    sprintf(ENDOF(buf2), "Willpower: %d (base willpower %d)\r\n", GET_WIL(ch), GET_REAL_WIL(ch));
  else
    sprintf(ENDOF(buf2), "Willpower: %d\r\n", GET_WIL(ch));
  
  if (GET_TRADITION(ch) == TRAD_MUNDANE)
    strcat(ENDOF(buf2), "As a Mundane, you have no magic.\r\n");
  else {
    if (GET_MAG(ch) != ch->real_abils.mag)
      sprintf(ENDOF(buf2), "Magic: %d (base magic %d)\r\n", (int)(GET_MAG(ch) / 100), MAX(0, (int)(ch->real_abils.mag / 100)));
    else
      sprintf(ENDOF(buf2), "Magic: %d\r\n", (int)(GET_MAG(ch) / 100));
    
    if (GET_TRADITION(ch) == TRAD_SHAMANIC)
      sprintf(ENDOF(buf2), "You follow %s.\r\n", totem_types[GET_TOTEM(ch)]);
    
    sprintf(ENDOF(buf2), "Initiation grade: %d\r\n", GET_GRADE(ch));
  }
  
  if (GET_REAL_REA(ch) != GET_REA(ch))
    sprintf(ENDOF(buf2), "Effective reaction: %d (real reaction %d)\r\n", GET_REA(ch), GET_REAL_REA(ch));
  else
    sprintf(ENDOF(buf2), "Reaction: %d\r\n", GET_REA(ch));
  
  sprintf(ENDOF(buf2), "Initiative: %d + %dd6\r\n", GET_REA(ch), 1 + GET_INIT_DICE(ch));
  
  return buf2;
}

const char *get_plaintext_score_essence(struct char_data *ch) {
  sprintf(buf2, "Essence: %.2f\r\n", ((float)GET_ESS(ch) / 100));
  sprintf(ENDOF(buf2), "Bioware Index: %.2f\r\n", ((float)GET_INDEX(ch) / 100));
  sprintf(ENDOF(buf2), "Essence Index: %.2f\r\n", ((float)GET_ESS(ch) / 100) + 3);
  return buf2;
}

const char *get_plaintext_score_equipment(struct char_data *ch) {
  sprintf(buf2, "Armor: %d ballistic, %d impact\r\n", GET_BALLISTIC(ch), GET_IMPACT(ch));
  sprintf(ENDOF(buf2), "Nuyen: %ld\r\n", GET_NUYEN(ch));
  sprintf(ENDOF(buf2), "You are carrying %.2f kilos. Your maximum carry weight is %d.\r\n", IS_CARRYING_W(ch), CAN_CARRY_W(ch));
  return buf2;
}

const char *get_plaintext_score_karma(struct char_data *ch) {
  sprintf(buf2, "Current karma: %.2f\r\n", ((float)GET_KARMA(ch) / 100));
  sprintf(ENDOF(buf2), "Total karma earned: %.2f\r\n", ((float)GET_TKE(ch)));
  sprintf(ENDOF(buf2), "Reputation: %d\r\n", GET_REP(ch));
  sprintf(ENDOF(buf2), "Notoriety: %d\r\n", GET_NOT(ch));
  return buf2;
}

const char *get_plaintext_score_misc(struct char_data *ch) {
  sprintf(buf2, "You are %s\r\n", get_position_string(ch));
  
  if (ch->desc != NULL && ch->desc->original != NULL ) {
    if (PLR_FLAGGED(ch->desc->original, PLR_MATRIX))
      sprintf(ENDOF(buf2), "You are connected to the Matrix.\r\n");
    else if (IS_PROJECT(ch))
      sprintf(ENDOF(buf2), "You are astrally projecting.\r\n");
    else
      sprintf(ENDOF(buf2), "You are occupying the body of %s.\r\n", GET_NAME(ch));
  }
  
  strcpy(ENDOF(buf2), get_vision_string(ch));
  
  if (IS_AFFECTED(ch, AFF_INVISIBLE) || IS_AFFECTED(ch, AFF_IMP_INVIS) || IS_AFFECTED(ch, AFF_SPELLINVIS) || IS_AFFECTED(ch, AFF_SPELLIMPINVIS))
    strcpy(ENDOF(buf2), "You are invisible.\r\n");
  
#ifdef ENABLE_HUNGER
  if (GET_COND(ch, COND_FULL) == 0)
    strcpy(ENDOF(buf2), "You are hungry.\r\n");
  
  if (GET_COND(ch, COND_THIRST) == 0)
    strcpy(ENDOF(buf2), "You are thirsty.\r\n");
#endif
  
  if (GET_COND(ch, COND_DRUNK) > 10)
    strcpy(ENDOF(buf2), "You are intoxicated.\r\n");
  
  if (AFF_FLAGGED(ch, AFF_SNEAK))
    sprintf(ENDOF(buf2), "You are sneaking.\r\n");
  
  // Physical and misc attributes.
  sprintf(ENDOF(buf2), "Height: %.2f meters\r\n", ((float)GET_HEIGHT(ch) / 100));
  sprintf(ENDOF(buf2), "Weight: %d kilos\r\n", GET_WEIGHT(ch));
  
  
  struct time_info_data playing_time;
  struct time_info_data real_time_passed(time_t t2, time_t t1);
  playing_time = real_time_passed(time(0) + ch->player.time.played, ch->player.time.logon);
  
  sprintf(ENDOF(buf2), "Current session length: %d days, %d hours.\r\n", playing_time.day, playing_time.hours);
  
  return buf2;
}

const char *get_plaintext_score_combat(struct char_data *ch) {
  strcpy(buf3, get_plaintext_score_health(ch));
  
  sprintf(ENDOF(buf3), "Initiative: %d + %dd6\r\n", GET_REA(ch), 1 + GET_INIT_DICE(ch));
  
  sprintf(ENDOF(buf3), "Armor: %d ballistic, %d impact\r\n", GET_BALLISTIC(ch), GET_IMPACT(ch));
  
  strcat(buf3, get_vision_string(ch));
  
  sprintf(ENDOF(buf3), "You are %s\r\n", get_position_string(ch));
  
  return buf3;
}

// Set of score switch possibilities.
struct score_switch_struct {
  const char *cmd;
  const char * (*command_pointer) (struct char_data *ch);
  bool hidden_in_help;
} score_switches[] = {
  { "attributes" , get_plaintext_score_stats    , FALSE },
  { "armor"      , get_plaintext_score_equipment, TRUE  }, // Alias for SCORE EQUIPMENT
  { "bioware"    , get_plaintext_score_essence  , TRUE  }, // Alias for SCORE ESSENCE
  { "cyberware"  , get_plaintext_score_essence  , TRUE  }, // Alias for SCORE ESSENCE
  { "combat"     , get_plaintext_score_combat   , FALSE },
  { "essence"    , get_plaintext_score_essence  , FALSE },
  { "equipment"  , get_plaintext_score_equipment, FALSE },
  { "gear"       , get_plaintext_score_equipment, TRUE  }, // Alias for SCORE EQUIPMENT
  { "health"     , get_plaintext_score_health   , FALSE },
  { "karma"      , get_plaintext_score_karma    , FALSE },
  { "mana"       , get_plaintext_score_health   , TRUE  }, // Alias for SCORE HEALTH
  { "mental"     , get_plaintext_score_health   , TRUE  }, // Alias for SCORE HEALTH
  { "misc"       , get_plaintext_score_misc     , FALSE },
  { "notoriety"  , get_plaintext_score_karma    , TRUE  }, // Alias for SCORE KARMA
  { "physical"   , get_plaintext_score_health   , TRUE  }, // Alias for SCORE HEALTH
  { "reputation" , get_plaintext_score_karma    , TRUE  }, // Alias for SCORE KARMA
  { "stats"      , get_plaintext_score_stats    , TRUE  }, // Alias for SCORE ATTRIBUTES
  { "\n"         , 0                            , TRUE  } // This must be last.
};

ACMD(do_score)
{
  struct time_info_data playing_time;
  struct time_info_data real_time_passed(time_t t2, time_t t1);
  char screenreader_buf[MAX_STRING_LENGTH];
  
  if ( IS_NPC(ch) && ch->desc == NULL )
    return;
  
  if (AFF_FLAGGED(ch, AFF_RIG) || PLR_FLAGGED(ch, PLR_REMOTE)) {
    struct veh_data *veh;
    RIG_VEH(ch, veh);
    if (PRF_FLAGGED(ch, PRF_SCREENREADER)) {
      sprintf(screenreader_buf,
              "You are rigging %s.\r\n"
              "Damage:^R%3d/10^n\r\n"
              "Mental:^B%3d(%2d)^n\r\n"
              "Reaction:%3d\r\n"
              "Int:%3d\r\n"
              "Wil:%3d\r\n"
              "Bod:%3d\r\n"
              "Armor:%3d\r\n"
              "Autonav:%3d\r\n"
              "Handling:%3d\r\n"
              "Speed:%3d\r\n"
              "Accel:%3d\r\n"
              "Sig:%3d\r\n"
              "Sensors:%3d\r\n",
              GET_VEH_NAME(veh), veh->damage, (int)(GET_MENTAL(ch) / 100),
              (int)(GET_MAX_MENTAL(ch) / 100), GET_REA(ch), GET_INT(ch),
              GET_WIL(ch), veh->body, veh->armor, veh->autonav,
              veh->handling, veh->speed, veh->accel, veh->sig,
              veh->sensor);
    } else {
      sprintf(buf, "You are rigging %s.\r\n", GET_VEH_NAME(veh));
      sprintf(ENDOF(buf), "Damage:^R%3d/10^n      Mental:^B%3d(%2d)^n\r\n"
              "  Reaction:%3d      Int:%3d\r\n"
              "       Wil:%3d      Bod:%3d\r\n"
              "     Armor:%3d  Autonav:%3d\r\n"
              "  Handling:%3d    Speed:%3d\r\n"
              "     Accel:%3d      Sig:%3d\r\n"
              "   Sensors:%3d\r\n",
              veh->damage, (int)(GET_MENTAL(ch) / 100),
              (int)(GET_MAX_MENTAL(ch) / 100), GET_REA(ch), GET_INT(ch),
              GET_WIL(ch), veh->body, veh->armor, veh->autonav,
              veh->handling, veh->speed, veh->accel, veh->sig,
              veh->sensor);
    }
  } else {
    // Switches for the specific score types.
    if (*argument) {
      int cmd_index;
      
      skip_spaces(&argument);
      
      // Find the index of the command the player wants.
      for (cmd_index = 0; *(score_switches[cmd_index].cmd) != '\n'; cmd_index++)
        if (!strncmp(argument, score_switches[cmd_index].cmd, strlen(argument)))
          break;
      
      // Precondition: If the command was invalid, show help and exit.
      if (*(score_switches[cmd_index].cmd) == '\n') {
        send_to_char("Sorry, that's not a valid score type. Available score types are:\r\n", ch);
        for (cmd_index = 0; *(score_switches[cmd_index].cmd) != '\n'; cmd_index++)
          if (!score_switches[cmd_index].hidden_in_help)
            send_to_char(ch, "  %s\r\n", score_switches[cmd_index].cmd);
        return;
      }
      
      // Execute the selected command, return its output, and exit.
      send_to_char(((*score_switches[cmd_index].command_pointer) (ch)), ch);
      return;
    }
    
    sprintf(buf, "^b//^L//^b//^L//^b//^L//^b//^L//^b//^L//^b//^L//^b//^L//^b//^L//^b//^L//^b//^L//"
                 "^b//^L//^b//^L//^b//^L//^b//^L^L//^b//^L//^b//^L//^b//^L//^b//^L/\r\n"
                 "^b/^L/  ^L \\_\\                                 ^rcondition monitor           ^L/^b/\r\n");
    
    sprintf(screenreader_buf, "Score sheet for %s:\r\n", GET_CHAR_NAME(ch));
    
    strcat(buf, "^L/^b/^L `//-\\\\                      ^gMent: ");
    int mental = GET_MENTAL(ch), physical = GET_PHYSICAL(ch);
    bool pain_editor = FALSE;
    for (struct obj_data *bio = ch->bioware; bio; bio = bio->next_content)
      if (GET_OBJ_VAL(bio, 0) == BIO_DAMAGECOMPENSATOR) {
        mental += GET_OBJ_VAL(bio, 1) * 100;
        physical += GET_OBJ_VAL(bio, 1) * 100;
      } else if (GET_OBJ_VAL(bio, 0) == BIO_PAINEDITOR && GET_OBJ_VAL(bio, 3)) {
        pain_editor = TRUE;
        mental = 1000;
        physical = 1000;
        break;
      }
    if (pain_editor) {
      strcat(buf, " ^YMasked by pain editor.      ^b/^L/\r\n");
    } else {
      if (mental >= 900 && mental < 1000)
        strcat(buf, "^b[^R*^b]");
      else
        strcat(buf, "^b[^gL^b]");
      if (mental >= 800 && mental < 900)
        strcat(buf, "^b[^R*^b]");
      else
        strcat(buf, "^b[ ]");
      if (mental >= 700 && mental < 800)
        strcat(buf, "^b[^R*^b]");
      else
        strcat(buf, "^b[^yM^b]");
      if (mental >= 600 && mental < 700)
        strcat(buf, "^b[^R*^b]");
      else
        strcat(buf, "^b[ ]");
      if (mental >= 500 && mental < 600)
        strcat(buf, "^b[^R*^b]");
      else
        strcat(buf, "^b[ ]");
      if (mental >= 400 && mental < 500)
        strcat(buf, "^b[^R*^b]");
      else
        strcat(buf, "^b[^rS^b]");
      if (mental >= 300 && mental < 400)
        strcat(buf, "^b[^R*^b]");
      else
        strcat(buf, "^b[ ]");
      if (mental >= 200 && mental < 300)
        strcat(buf, "^b[^R*^b]");
      else
        strcat(buf, "^b[ ]");
      if (mental >= 100 && mental < 200)
        strcat(buf, "^b[^R*^b]");
      else
        strcat(buf, "^b[ ]");
      if (mental < 100)
        strcat(buf, "^b[^R*^b]");
      else
        strcat(buf, "^b[^RD^b]");
      strcat(buf, "  ^b/^L/\r\n");
    }
      
    strcat(buf, "^b/^L/ ^L`\\\\-\\^wHADOWRUN 3rd Edition   ^rPhys: ");
    if (pain_editor) {
      strcat(buf, " ^YMasked by pain editor.        ^L/^b/\r\n");
    } else {
      if (physical >= 900 && physical < 1000)
        strcat(buf, "^L[^R*^L]");
      else
        strcat(buf, "^L[^gL^L]");
      if (physical >= 800 && physical < 900)
        strcat(buf, "^L[^R*^L]");
      else
        strcat(buf, "^L[ ]");
      if (physical >= 700 && physical < 800)
        strcat(buf, "^L[^R*^L]");
      else
        strcat(buf, "^L[^yM^L]");
      if (physical >= 600 && physical < 700)
        strcat(buf, "^L[^R*^L]");
      else
        strcat(buf, "^L[ ]");
      if (physical >= 500 && physical < 600)
        strcat(buf, "^L[^R*^L]");
      else
        strcat(buf, "^L[ ]");
      if (physical >= 400 && physical < 500)
        strcat(buf, "^L[^R*^L]");
      else
        strcat(buf, "^L[^rS^L]");
      if (physical >= 300 && physical < 400)
        strcat(buf, "^L[^R*^L]");
      else
        strcat(buf, "^L[ ]");
      if (physical >= 200 && physical < 300)
        strcat(buf, "^L[^R*^L]");
      else
        strcat(buf, "^L[ ]");
      if (physical >= 100 && physical < 200)
        strcat(buf, "^L[^R*^L]");
      else
        strcat(buf, "^L[ ]");
      if (physical < 100)
        strcat(buf, "^L[^R*^L]");
      else
        strcat(buf, "^L[^RD^L]");
      strcat(buf, "  ^L/^b/\r\n");
    }
    
    if (pain_editor) {
      strcat(buf, "^L/^b/  ^L///-\\  ^wcharacter sheet                                           ^b/^L/\r\n");
    } else {
      strcat(buf, "^L/^b/  ^L///-\\  ^wcharacter sheet           ^LPhysical Damage Overflow: ");
      if (physical < 0)
        sprintf(ENDOF(buf), "^R[%2d]  ^b/^L/\r\n", (int)(physical / 100) * -1);
      else
        strcat(buf, "^R[ 0]  ^b/^L/\r\n");
    }
    
    /* Calculate the various bits of data that the score sheet needs. */
    
    playing_time = real_time_passed(time(0) + ch->player.time.played, ch->player.time.logon);
    
    static char out_of_body_string[200];
    if (ch->desc != NULL && ch->desc->original != NULL ) {
      if (PLR_FLAGGED(ch->desc->original, PLR_MATRIX))
        sprintf(out_of_body_string, "You are connected to the Matrix.");
      else if (IS_PROJECT(ch))
        sprintf(out_of_body_string, "You are astrally projecting.");
      else
        sprintf(out_of_body_string, "You are occupying the body of %s.", GET_NAME(ch));
    } else {
      strcpy(out_of_body_string, "");
    }
    
    static char invisibility_string[50];
    if (IS_AFFECTED(ch, AFF_INVISIBLE) || IS_AFFECTED(ch, AFF_IMP_INVIS) || IS_AFFECTED(ch, AFF_SPELLINVIS) || IS_AFFECTED(ch, AFF_SPELLIMPINVIS))
      strcpy(invisibility_string, "You are invisible.");
    else
      strcpy(invisibility_string, "");
    
    
    static char shaman_string[50];
    if (GET_TRADITION(ch) == TRAD_SHAMANIC)
      sprintf(shaman_string, "You follow %s.", totem_types[GET_TOTEM(ch)]);
    else
      strcpy(shaman_string, "");
    
    static char grade_string[50];
    if (GET_TRADITION(ch) != TRAD_MUNDANE)
      sprintf(grade_string, "^nGrade: ^w[^W%2d^w]", GET_GRADE(ch));
    else
      strcpy(grade_string, "");
    
    sprintf(ENDOF(buf), "^b/^L/  ^L\\\\@//                                                            ^L/^b/\r\n");
    sprintf(ENDOF(buf), "^L/^b/   ^L`^                                                              ^b/^L/\r\n");
    sprintf(ENDOF(buf), "^b/^L/                                                                   ^L/^b/\r\n");
    sprintf(ENDOF(buf), "^L/^b/ ^nBody          ^w%2d (^W%2d^w)    Height: ^W%0.2f^w meters   Weight: ^W%3d^w kilos  ^b/^L/\r\n",
                          GET_REAL_BOD(ch), GET_BOD(ch), ((float)GET_HEIGHT(ch) / 100), GET_WEIGHT(ch));
    sprintf(ENDOF(buf), "^b/^L/ ^nQuickness     ^w%2d (^W%2d^w)    Encumbrance: ^W%3.2f^w kilos carried, ^W%3d^w max ^L/^b/\r\n",
                          GET_REAL_QUI(ch), GET_QUI(ch), IS_CARRYING_W(ch) ,CAN_CARRY_W(ch));
    sprintf(ENDOF(buf), "^L/^b/ ^nStrength      ^w%2d (^W%2d^w)    Current session length ^W%2d^w days, ^W%2d^w hours.^b/^L/\r\n",
                          GET_REAL_STR(ch), GET_STR(ch), playing_time.day, playing_time.hours);
    sprintf(ENDOF(buf), "^b/^L/ ^nCharisma      ^w%2d (^W%2d^w)    ^wKarma ^B[^W%7.2f^B] ^wRep ^B[^W%4d^B] ^rNotor ^r[^R%4d^r]  ^L/^b/\r\n",
                          GET_REAL_CHA(ch), GET_CHA(ch), ((float)GET_KARMA(ch) / 100), GET_REP(ch), GET_NOT(ch));
    sprintf(ENDOF(buf), "^L/^b/ ^nIntelligence  ^w%2d (^W%2d^w)    ^r%-41s^b/^L/\r\n",
                          GET_REAL_INT(ch), GET_INT(ch), get_vision_string(ch, TRUE));
    sprintf(ENDOF(buf), "^b/^L/ ^nWillpower     ^w%2d (^W%2d^w)    ^nYou are %-33s^L/^b/\r\n",
                          GET_REAL_WIL(ch), GET_WIL(ch), get_position_string(ch));
    sprintf(ENDOF(buf), "^L/^b/ ^nEssence       ^g[^w%5.2f^g]    ^W%-18s                       ^b/^L/\r\n",
                          ((float)GET_ESS(ch) / 100), invisibility_string);
#ifdef ENABLE_HUNGER
    sprintf(ENDOF(buf), "^b/^L/ ^nBioware Index ^B[^w%5.2f^B]    ^n%-15s                          ^L/^b/\r\n",
                          ((float)GET_INDEX(ch) / 100), GET_COND(ch, COND_FULL) == 0 ? "You are hungry." : "");
    sprintf(ENDOF(buf), "^L/^b/ ^nEssence Index ^W[^w%5.2f^W]    ^n%-16s                         ^b/^L/\r\n",
                          (((float)GET_ESS(ch) / 100) + 3), GET_COND(ch, COND_THIRST) == 0 ? "You are thirsty." : "");
#else
    sprintf(ENDOF(buf), "^b/^L/ ^nBioware Index ^B[^w%5.2f^B]    ^n%-15s                          ^L/^b/\r\n",
                          ((float)GET_INDEX(ch) / 100), "");
    sprintf(ENDOF(buf), "^L/^b/ ^nEssence Index ^W[^w%5.2f^W]    ^n%-16s                         ^b/^L/\r\n",
                          (((float)GET_ESS(ch) / 100) + 3), "");
#endif
    sprintf(ENDOF(buf), "^b/^L/ ^nMagic         ^w%2d (^W%2d^w)    ^g%-20s                     ^L/^b/\r\n",
                          MAX(0, ((int)ch->real_abils.mag / 100)), ((int)GET_MAG(ch) / 100), GET_COND(ch, COND_DRUNK) > 10 ? "You are intoxicated." : "");
    sprintf(ENDOF(buf), "^L/^b/ ^nReaction      ^w%2d (^W%2d^w)    ^c%-41s^b/^L/\r\n",
                          GET_REAL_REA(ch), GET_REA(ch), out_of_body_string);
    sprintf(ENDOF(buf), "^b/^L/ ^nInitiative^w   [^W%2d^w+^W%d^rd6^n]    ^n%-32s         ^L/^b/\r\n",
                          GET_REA(ch), 1 + GET_INIT_DICE(ch), shaman_string);
    sprintf(ENDOF(buf), "^L/^b/ ^nArmor     ^w[ ^W%2d^rB^w/ ^W%2d^rI^w]    ^L%-17s^n                        ^b/^L/\r\n",
                          GET_BALLISTIC(ch), GET_IMPACT(ch), AFF_FLAGGED(ch, AFF_SNEAK) ? "You are sneaking." : "");
    sprintf(ENDOF(buf), "^b/^L/ ^nNuyen     ^w[^W%'9ld^w]    %11s                              ^L/^b/\r\n",
                          GET_NUYEN(ch), grade_string);
    strcat(buf, "^L/^b/                                                                   ^b/^L/\r\n"
            "^L//^b//^L//^b//^L//^b//^L//^b//^L//^b//^L//^b//^L//^b//"
            "^L//^b//^L//^b//^L//^b//^L//^b//^L//^b//^L//^b//^L//"
            "^b//^L^L//^b//^L//^b//^L//^b//^L//^b/\r\n");
    
    // Compose the remainder of the screenreader score.
    
    // Health:
    strcat(screenreader_buf, get_plaintext_score_health(ch));
    
    // Attributes:
    strcat(screenreader_buf, get_plaintext_score_stats(ch));
    
    // Derived stats:
    strcat(screenreader_buf, get_plaintext_score_essence(ch));
    
    // Equipment attributes:
    strcat(screenreader_buf, get_plaintext_score_equipment(ch));
    
    // Karma stats.
    strcat(screenreader_buf, get_plaintext_score_karma(ch));
    
    // Condition and misc affects.
    strcat(screenreader_buf, get_plaintext_score_misc(ch));
  }
  if (PRF_FLAGGED(ch, PRF_SCREENREADER))
    send_to_char(screenreader_buf, ch);
  else
    send_to_char(buf, ch);
}

ACMD(do_inventory)
{
  send_to_char("You are carrying:\r\n", ch);
  list_obj_to_char(ch->carrying, ch, 1, TRUE, FALSE);
}

ACMD(do_cyberware)
{
  struct obj_data *obj;
  
  if (!ch->cyberware) {
    send_to_char("You have no cyberware.\r\n", ch);
    return;
  }
  
  send_to_char("You have the following cyberware:\r\n", ch);
  for (obj = ch->cyberware; obj != NULL; obj = obj->next_content) {
    sprintf(buf, "%-30s Essence: %0.2f\r\n",
            GET_OBJ_NAME(obj), ((float)GET_OBJ_VAL(obj, 4) / 100));
    send_to_char(buf, ch);
  }
}

ACMD(do_bioware)
{
  struct obj_data *obj;
  
  if (!ch->bioware) {
    send_to_char("You have no bioware.\r\n", ch);
    return;
  }
  
  send_to_char("You have the following bioware:\r\n", ch);
  for (obj = ch->bioware; obj != NULL; obj = obj->next_content) {
    sprintf(buf, "%-30s Rating: %-2d     Body Index: %0.2f\r\n",
            GET_OBJ_NAME(obj),
            GET_OBJ_VAL(obj, 1), ((float)GET_OBJ_VAL(obj, 4) / 100));
    send_to_char(buf, ch);
  }
}

ACMD(do_equipment)
{
  int i, found = 0;
  
  send_to_char("You are using:\r\n", ch);
  for (i = 0; i < NUM_WEARS; i++) {
    if (GET_EQ(ch, i)) {
      if (i == WEAR_WIELD || i == WEAR_HOLD) {
        if (IS_OBJ_STAT(GET_EQ(ch, i), ITEM_TWOHANDS))
          send_to_char(hands[2], ch);
        else if (GET_OBJ_TYPE(GET_EQ(ch, i)) == ITEM_WEAPON) { // wielding something?
          if (i == WEAR_WIELD)
            send_to_char(wielding_hands[(int)ch->char_specials.saved.left_handed], ch);
          else
            send_to_char(wielding_hands[!ch->char_specials.saved.left_handed], ch);
        } else {                                              // just held
          if (i == WEAR_WIELD)
            send_to_char(hands[(int)ch->char_specials.saved.left_handed], ch);
          else
            send_to_char(hands[!ch->char_specials.saved.left_handed], ch);
        }
      } else
        send_to_char(where[i], ch);
      if (CAN_SEE_OBJ(ch, GET_EQ(ch, i)))
        show_obj_to_char(GET_EQ(ch, i), ch, 7);
      else
        send_to_char("Something.\r\n", ch);
      found = TRUE;
    }
  }
  if (!found) {
    send_to_char(" Nothing.\r\n", ch);
  }
}

ACMD_CONST(do_time) {
  char not_const[MAX_STRING_LENGTH];
  strcpy(not_const, argument);
  ACMD_DECLARE(do_time);
  do_time(ch, not_const, cmd, subcmd);
}

ACMD(do_time)
{
  sh_int year, month, day, hour, minute, pm;
  extern struct time_info_data time_info;
  extern const char *weekdays[];
  extern const char *month_name[];
  struct obj_data *check;
  
  if (subcmd == SCMD_NORMAL && GET_REAL_LEVEL(ch) >= LVL_BUILDER)
    subcmd = SCMD_PRECISE;
  for (check = ch->cyberware; check; check = check->next_content)
    if (GET_OBJ_VAL(check, 0) == CYB_MATHSSPU || (GET_OBJ_VAL(check, 0) == CYB_EYES && IS_SET(GET_OBJ_VAL(check, 3), EYE_CLOCK)))
      subcmd = SCMD_PRECISE;
  
  year = time_info.year % 100;
  month = time_info.month + 1;
  day = time_info.day + 1;
  hour = (time_info.hours % 12 == 0 ? 12 : time_info.hours % 12);
  minute = time_info.minute;
  pm = (time_info.hours >= 12);
  
  if (subcmd == SCMD_NORMAL)
    sprintf(buf, "%d o'clock %s, %s, %s %d, %d.\r\n", hour, pm ? "PM" : "AM",
            weekdays[(int)time_info.weekday], month_name[month - 1], day, time_info.year);
  else
    sprintf(buf, "%d:%s%d %s, %s, %s%d/%s%d/%d.\r\n", hour,
            minute < 10 ? "0" : "", minute, pm ? "PM" : "AM",
            weekdays[(int)time_info.weekday], month < 10 ? "0" : "", month,
            day < 10 ? "0" : "", day, year);
  
  
  send_to_char(buf, ch);
}

ACMD(do_weather)
{
  static const char *sky_look[] =
  {
    "cloudless",
    "cloudy",
    "rainy",
    "lit by flashes of lightning"
  };
  
  if (OUTSIDE(ch)) {
    sprintf(buf, "The sky is %s and %s.\r\n", sky_look[weather_info.sky],
            (weather_info.change >= 0 ? "you feel a warm wind from south" :
             "your foot tells you bad weather is due"));
    send_to_char(buf, ch);
  } else
    send_to_char("You have no feeling about the weather at all.\r\n", ch);
}


// this command sends the index to the player--it can take a letter as an
// argument and will send only words that fall under that letter
ACMD(do_index)
{
  char *temp = argument;
  char query[MAX_STRING_LENGTH];
  MYSQL_RES *res;
  MYSQL_ROW row;
  
  skip_spaces(&temp);
  
  if (!*temp) {
    sprintf(query, "SELECT name FROM help_topic ORDER BY name ASC");
  } else {
    char letter = *temp;
    if (!isalpha(letter)) {
      send_to_char("Only letters can be sent to the index command.\r\n", ch);
      return;
    }
    
    letter = tolower(letter);
    
    // No prepare_quotes since this is guaranteed to be a single alphanumeric character.
    sprintf(query, "SELECT name FROM help_topic WHERE name LIKE '%c%%'", letter);
  }
  
  // Execute the query and check for errors.
  if (mysql_wrapper(mysql, query)) {
    send_to_char("Apologies, but it seems the help system is currently offline.\r\n", ch);
    mudlog("WARNING: Failed to return all help topics with 'index' command.", ch, LOG_SYSLOG, TRUE);
    return;
  }
  
  // Build the index and send it to the character.
  send_to_char("The following help topics are available:\r\n", ch);
  res = mysql_store_result(mysql);
  while ((row = mysql_fetch_row(res))) {
    send_to_char(ch, " %s\r\n", row[0]);
  }
  
  send_to_char("You can see more about these with HELP <topic>.\r\n", ch);
  
  // Clean up.
  mysql_free_result(res);
}

void display_help(char *help, const char *arg) {
  char query[MAX_STRING_LENGTH];
  MYSQL_RES *res;
  MYSQL_ROW row;
  *help = '\0';
  
  // Buf now holds the quoted version of arg.
  prepare_quotes(buf, arg, sizeof(buf) / sizeof(buf[0]));
  
  // First strategy: Look for an exact match.
  sprintf(query, "SELECT * FROM help_topic WHERE name='%s'", buf);
  if (mysql_wrapper(mysql, query)) {
    // We got a SQL error-- bail.
    sprintf(help, "The help system is temporarily unavailable.\r\n");
    mudlog("WARNING: Failed to return help topic. See server log for MYSQL error.", NULL, LOG_SYSLOG, TRUE);
    return;
  } else {
    res = mysql_store_result(mysql);
    if ((row = mysql_fetch_row(res))) {
      // Found it-- send it back and have done with it.
      sprintf(help, "^W%s^n\r\n\r\n%s\r\n", row[0], row[1]);
      mysql_free_result(res);
      return;
    } else {
      // Failed to find a match-- clean up and continue.
      mysql_free_result(res);
    }
  }
  
  // Second strategy: Search for possible like-matches.
  sprintf(query, "SELECT * FROM help_topic WHERE name LIKE '%%%s%%' ORDER BY name ASC", buf);
  if (mysql_wrapper(mysql, query)) {
    // If we don't find it here either, we know the file doesn't exist-- failure condition.
    sprintf(help, "No such help file exists.\r\n");
    return;
  }
  res = mysql_store_result(mysql);
  int x = mysql_num_rows(res);
  
  // If we have no rows, fail.
  if (x < 1) {
    sprintf(help, "No such help file exists.\r\n");
  }
  
  // If we have too many rows, try to refine the search to just files starting with the search string.
  else if (x > 5) {
    mysql_free_result(res);
    sprintf(query, "SELECT * FROM help_topic WHERE name LIKE '%s%%' ORDER BY name ASC", buf);
    if (mysql_wrapper(mysql, query)) {
      sprintf(help, "%d articles returned, please narrow your search.aa\r\n", x);
      return;
    }
    res = mysql_store_result(mysql);
    row = mysql_fetch_row(res);
    int y = mysql_num_rows(res);
    if (y == 1)
      sprintf(ENDOF(help), "^W%s^n\r\n\r\n%s\r\n", row[0], row[1]);
    else sprintf(help, "%d articles returned, please narrow your search.\r\n", x);
  } else {
    while ((row = mysql_fetch_row(res))) {
      sprintf(ENDOF(help), "^W%s^n\r\n\r\n%s\r\n", row[0], row[1]);
    }
    return;
  }
  mysql_free_result(res);
}

ACMD(do_help)
{
  extern char *help;
  char buf[MAX_STRING_LENGTH*2];
  skip_spaces(&argument);
  
  if (!*argument) {
    page_string(ch->desc, help, 0);
    return;
  }
  
  display_help(buf, argument);
  send_to_char(buf, ch);
}

ACMD(do_wizhelp)
{
  struct char_data *vict = NULL;
  int no, cmd_num;
  
  if (!ch->desc)
    return;
  
  skip_spaces(&argument);
  
  if (!*argument || ((vict = get_char_vis(ch, argument)) && !IS_NPC(vict) &&
                     access_level(vict, LVL_BUILDER))) {
    if (!vict)
      vict = ch;
    sprintf(buf, "The following privileged commands are available to %s:\r\n",
            vict == ch ? "you" : GET_CHAR_NAME(vict));
    
    /* cmd_num starts at 1, not 0, to remove 'RESERVED' */
    for (no = 1, cmd_num = 1; *cmd_info[cmd_num].command != '\n'; cmd_num++)
      if (cmd_info[cmd_num].minimum_level >= LVL_BUILDER
          && GET_LEVEL(vict) >= cmd_info[cmd_num].minimum_level) {
        sprintf(buf + strlen(buf), "%-13s", cmd_info[cmd_num].command);
        if (!(no % 6))
          strcat(buf, "\r\n");
        no++;
      }
    strcat(buf, "\r\n");
    send_to_char(buf, ch);
    return;
  }
  
  return;
  /*
   struct char_data *vict = NULL;
   int no, cmd_num;
   
   if (!ch->desc)
   return;
   
   skip_spaces(&argument);
   
   if (!*argument || ((vict = get_char_vis(ch, argument)) && !IS_NPC(vict) &&
   access_level(vict, LVL_BUILDER))) {
   if (!vict)
   vict = ch;
   sprintf(buf, "The following privileged commands are available to %s:\r\n",
   vict == ch ? "you" : GET_CHAR_NAME(vict));
   
   for (no = 1, cmd_num = 1; *cmd_info[cmd_num].command != '\n'; cmd_num++)
   if (cmd_info[cmd_num].minimum_level >= LVL_BUILDER
   && GET_LEVEL(vict) >= cmd_info[cmd_num].minimum_level) {
   sprintf(buf + strlen(buf), "%-13s", cmd_info[cmd_num].command);
   if (!(no % 6))
   strcat(buf, "\r\n");
   no++;
   }
   strcat(buf, "\r\n");
   send_to_char(buf, ch);
   return;
   }
   if (WizHelp.FindTopic(buf, argument))
   send_to_char(buf, ch);
   else
   send_to_char("That help topic doesn't exist.\r\n", ch);
   */
}

#define WHO_FORMAT "format: who [sort] [quest] [pker] [immortal] [mortal] [hardcore] [ooc] [newbie]\r\n"

ACMD_CONST(do_who) {
  char not_const[MAX_STRING_LENGTH];
  strcpy(not_const, argument);
  ACMD_DECLARE(do_who);
  do_who(ch, not_const, cmd, subcmd);
}

ACMD(do_who)
{
  struct descriptor_data *d;
  struct char_data *tch;
  int sort = LVL_MAX, num_can_see = 0, level = GET_LEVEL(ch);
  bool mortal = FALSE, hardcore = FALSE, quest = FALSE, pker = FALSE, immort = FALSE, ooc = FALSE, newbie = FALSE;
  int output_header;
  
  skip_spaces(&argument);
  strcpy(buf, argument);
  if (subcmd)
    level = 1;
  while (*buf) {
    half_chop(buf, arg, buf1);
    
    if (is_abbrev(arg, "sort"))
      sort = LVL_MAX;
    else if (is_abbrev(arg, "quest"))
      quest = 1;
    else if (is_abbrev(arg, "pker"))
      pker = 1;
    else if (is_abbrev(arg, "immortal"))
      immort = 1;
    else if (is_abbrev(arg, "mortal"))
      mortal = 1;
    else if (is_abbrev(arg, "hardcore"))
      hardcore = 1;
    else if (is_abbrev(arg, "ooc"))
      ooc = 1;
    else if (is_abbrev(arg, "newbie"))
      newbie = 1;
    else {
      send_to_char(WHO_FORMAT, ch);
      return;
    }
    strcpy(buf, buf1);
  }                             /* end while (parser) */
  
  *buf = '\0';
  output_header = 1;
  
  for (; sort != 0; sort > 0 ? sort-- : sort++) {
    if ( sort == 1 )
      output_header = 1;
    for (d = descriptor_list; d; d = d->next) {
      if (d->connected)
        continue;
      
      if (d->original)
        tch = d->original;
      else if (!(tch = d->character))
        continue;
      
      if ((mortal && IS_SENATOR(tch)) ||
          (immort && !IS_SENATOR(tch)))
        continue;
      if (quest && !PRF_FLAGGED(tch, PRF_QUEST))
        continue;
      if (pker && !PRF_FLAGGED(tch, PRF_PKER))
        continue;
      if (hardcore && !PRF_FLAGGED(tch, PRF_HARDCORE))
        continue;
      if (sort > 0 && GET_LEVEL(tch) != sort)
        continue;
      if (ooc && (PRF_FLAGGED(tch, PRF_NOOOC) || PLR_FLAGGED(tch, PLR_AUTH)))
        continue;
      if (newbie && !PLR_FLAGGED(tch, PLR_NEWBIE))
        continue;
      if (GET_INCOG_LEV(tch) > GET_LEVEL(ch))
        continue;
      num_can_see++;
      
      if ( output_header ) {
        output_header = 0;
        if ( sort >= LVL_BUILDER )
          strcat(buf, "\r\n^W Immortals ^L: ^WStaff Online\r\n ^w---------------------------\r\n");
        else
          strcat(buf, "\r\n     ^W Race ^L: ^WVisible Players\r\n ^W---------------------------\r\n");
      }
      
      switch (GET_LEVEL(tch)) {
        case LVL_BUILDER:
        case LVL_ARCHITECT:
          sprintf(buf1, "^G");
          break;
        case LVL_FIXER:
        case LVL_CONSPIRATOR:
          sprintf(buf1, "^m");
          break;
        case LVL_EXECUTIVE:
          sprintf(buf1, "^c");
          break;
        case LVL_DEVELOPER:
          sprintf(buf1, "^r");
          break;
        case LVL_VICEPRES:
        case LVL_ADMIN:
          sprintf(buf1, "^b");
          break;
        case LVL_PRESIDENT:
          sprintf(buf1, "^B");
          break;
        default:
          sprintf(buf1, "^L");
          break;
      }
      if (PRF_FLAGGED(tch, PRF_SHOWGROUPTAG) && GET_PGROUP_MEMBER_DATA(tch) && GET_PGROUP(tch)) {
        sprintf(buf2, "%10s :^N %s%s^N%s%s%s %s^n",
                (GET_WHOTITLE(tch) ? GET_WHOTITLE(tch) : ""),
                (GET_PGROUP(tch)->get_tag()),
                (strlen(GET_PGROUP(tch)->get_tag()) ? " " : ""),
                (GET_PRETITLE(tch) ? GET_PRETITLE(tch) : ""),
                (GET_PRETITLE(tch) && strlen(GET_PRETITLE(tch)) ? " " : ""),
                GET_CHAR_NAME(tch),
                GET_TITLE(tch));
      } else {
        sprintf(buf2, "%10s :^N %s%s%s %s^n",
                (GET_WHOTITLE(tch) ? GET_WHOTITLE(tch) : ""),
                (GET_PRETITLE(tch) ? GET_PRETITLE(tch) : ""),
                (GET_PRETITLE(tch) && strlen(GET_PRETITLE(tch)) ? " " : ""),
                GET_CHAR_NAME(tch),
                GET_TITLE(tch));
      }
      strcat(buf1, buf2);
      
      if (GET_PGROUP_MEMBER_DATA(tch) && GET_PGROUP(tch) && !(GET_PGROUP(tch)->is_secret())
          && GET_PGROUP_MEMBER_DATA(ch)
          && GET_PGROUP(ch) == GET_PGROUP(tch)) {
        sprintf(buf2, " ^G(^g%s^G)^n", GET_PGROUP(tch)->get_alias());
        strcat(buf1, buf2);
      }
      
      if (PRF_FLAGGED(tch, PRF_AFK))
        strcat(buf1, " (AFK)");
      if (PLR_FLAGGED(tch, PLR_RPE) && (level > LVL_MORTAL || PLR_FLAGGED(ch, PLR_RPE)))
        strcat(buf1, " ^R(RPE)^n");
      
      if (PRF_FLAGGED(tch, PRF_NEWBIEHELPER) && (level > LVL_MORTAL || PRF_FLAGGED(ch, PRF_NEWBIEHELPER)))
        strcat(buf1, " ^G(Newbie Helper)^n");
      
      if (level > LVL_MORTAL) {
        if (GET_INVIS_LEV(tch) && level >= GET_INVIS_LEV(tch))
          sprintf(ENDOF(buf1), " (i%d)", GET_INVIS_LEV(tch));
        if (GET_INCOG_LEV(tch))
          sprintf(ENDOF(buf1), " (c%d)", GET_INCOG_LEV(tch));
        if (PLR_FLAGGED(tch, PLR_MAILING))
          strcat(buf1, " (mailing)");
        else if (PLR_FLAGGED(tch, PLR_WRITING))
          strcat(buf1, " (writing)");
        if (PLR_FLAGGED(tch, PLR_EDITING))
          strcat(buf1, " (editing)");
        if (PRF_FLAGGED(tch, PRF_QUESTOR))
          strcat(buf1, " ^Y(questor)^n");
        if (PLR_FLAGGED(tch, PLR_AUTH))
          strcat(buf1, " ^G(unauthed)^n");
        if (PLR_FLAGGED(tch, PLR_MATRIX))
          strcat(buf1, " (decking)");
        else if (PLR_FLAGGED(tch, PLR_PROJECT))
          strcat(buf1, " (projecting)");
        else if (d->original) {
          if (IS_NPC(d->character) && GET_MOB_VNUM(d->character) >= 50 &&
              GET_MOB_VNUM(d->character) < 70)
            strcat(buf1, " (morphed)");
          else
            strcat(buf1, " (switched)");
        }
      }
      if (!quest && PRF_FLAGGED(tch, PRF_QUEST))
        strcat(buf1, " ^Y(hired)^n");
      if (PRF_FLAGGED(tch, PRF_NOTELL))
        strcat(buf1, " (!tell)");
      if (PLR_FLAGGED(tch, PLR_KILLER))
        strcat(buf1, " ^R(KILLER)^N");
      if (PLR_FLAGGED(tch, PLR_BLACKLIST))
        strcat(buf1, " ^L(BLACKLISTED)^N");
      if (PLR_FLAGGED(tch, PLR_WANTED))
        strcat(buf1, " ^R(WANTED)^N");
      strcat(buf1, "\r\n");
      strcat(buf, buf1);
      
      CGLOB = KNRM;
    }
  }
  
  if (num_can_see == 0)
    strcat(buf, "No-one at all!\r\n");
  else if (num_can_see == 1)
    strcat(buf, "One lonely chummer displayed.\r\n");
  else
    sprintf(buf, "%s\r\n%d chummers displayed.\r\n", buf, num_can_see);
  if (subcmd) {
    FILE *fl;
    static char buffer[MAX_STRING_LENGTH*4];
    char *temp = &buffer[0];
    const char *color;
    char *str = str_dup(buf);
    char *ptr = str;
    while(*str) {
      if (*str == '^') {
        switch (*++str) {
          case 'l':
            color = "<span style=\"color:#000000\">";
            break; // black
          case 'r':
            color = "<span style=\"color:#990000\">";
            break; // red
          case 'g':
            color = "<span style=\"color:#336633\">";
            break; // green
          case 'y':
            color = "<span style=\"color:#CC9933\">";
            break; // yellow
          case 'b':
            color = "<span style=\"color:#3333CC\">";
            break; // blue
          case 'm':
            color = "<span style=\"color:#663366\">";
            break; // magenta
          case 'n':
            color = "<span style=\"color:#CCCCCC\">";
            break; // normal
          case 'c':
            color = "<span style=\"color:#009999\">";
            break; // cyan
          case 'w':
            color = "<span style=\"color:#CCCCCC\">";
            break; // white
          case 'L':
            color = "<span style=\"color:#666666\">";
            break; // bold black
          case 'R':
            color = "<span style=\"color:#FF0000\">";
            break; // bold red
          case 'G':
            color = "<span style=\"color:#00FF00\">";
            break; // bold green
          case 'Y':
            color = "<span style=\"color:#FFFF00\">";
            break; // bold yellow
          case 'B':
            color = "<span style=\"color:#0000FF\">";
            break; // bold blue
          case 'M':
            color = "<span style=\"color:#FF00FF\">";
            break; // bold magenta
          case 'N':
            color = "<span style=\"color:#CCCCCC\">";
            break;
          case 'C':
            color = "<span style=\"color:#00FFFF\">";
            break; // bold cyan
          case 'W':
            color = "<span style=\"color:#FFFFFF\">";
            break; // bold white
          case '^':
            color = "^";
            break;
          default:
            color = NULL;
            break;
        }
        if (color) {
          while (*color)
            *temp++ = *color++;
          ++str;
        } else {
          *temp++ = '^';
          *temp++ = *str++;
        }
      } else
        *temp++ = *str++;
    }
    *temp = '\0';
    DELETE_AND_NULL_ARRAY(ptr);
    if (!(fl = fopen("text/wholist", "w"))) {
      mudlog("SYSERR: Cannot open wholist for write", NULL, LOG_SYSLOG, FALSE);
      return;
    }
    fprintf(fl, "<HTML><BODY bgcolor=#41494C><PRE>%s</PRE></BODY></HTML>\r\n", &buffer[0]);
    fclose(fl);
  } else page_string(ch->desc, buf, 1);
}

#define USERS_FORMAT "format: users [-n name] [-h host] [-o] [-p]\r\n"

ACMD(do_users)
{
  extern const char *connected_types[];
  char line[200], line2[220], idletime[10];
  char state[30], *timeptr, mode;
  const char *format;
  char name_search[MAX_INPUT_LENGTH], host_search[MAX_INPUT_LENGTH];
  struct char_data *tch;
  struct descriptor_data *d;
  int num_can_see = 0;
  int outlaws = 0, playing = 0, deadweight = 0;
  
  host_search[0] = name_search[0] = '\0';
  
  strcpy(buf, argument);
  while (*buf) {
    half_chop(buf, arg, buf1);
    if (*arg == '-') {
      mode = *(arg + 1);  /* just in case; we destroy arg in the switch */
      switch (mode) {
        case 'o':
        case 'k':
          outlaws = 1;
          playing = 1;
          strcpy(buf, buf1);
          break;
        case 'p':
          playing = 1;
          strcpy(buf, buf1);
          break;
        case 'd':
          deadweight = 1;
          strcpy(buf, buf1);
          break;
        case 'n':
          playing = 1;
          half_chop(buf1, name_search, buf);
          break;
        case 'h':
          playing = 1;
          half_chop(buf1, host_search, buf);
          break;
        default:
          send_to_char(USERS_FORMAT, ch);
          return;
      }                         /* end of switch */
      
    } else {                    /* endif */
      send_to_char(USERS_FORMAT, ch);
      return;
    }
  }                             /* end while (parser) */
  strcpy(line, "Num  Name           State           Idle Login@   Site\r\n");
  strcat(line, "---- -------------- --------------- ---- -------- ---------------------------\r\n");
  send_to_char(line, ch);
  
  one_argument(argument, arg);
  
  for (d = descriptor_list; d; d = d->next) {
    if (d->connected && playing)
      continue;
    if (!d->connected && deadweight)
      continue;
    if (!d->connected) {
      if (d->original)
        tch = d->original;
      else if (!(tch = d->character))
        continue;
      
      if (*host_search && !strstr((const char *)d->host, host_search))
        continue;
      if (*name_search && isname(name_search, GET_KEYWORDS(tch)))
        continue;
      if (!CAN_SEE(ch, tch))
        continue;
      if (outlaws &&
          !PLR_FLAGGED(tch, PLR_KILLER))
        continue;
      if (GET_INVIS_LEV(tch) > GET_REAL_LEVEL(ch))
        continue;
      if (GET_INCOG_LEV(tch) > GET_REAL_LEVEL(ch))
        continue;
    }
    
    timeptr = asctime(localtime(&d->login_time));
    timeptr += 11;
    *(timeptr + 8) = '\0';
    
    if (!d->connected && d->original)
      strcpy(state, "Switched");
    else
      strcpy(state, connected_types[d->connected]);
    
    if (d->character && !d->connected)
      sprintf(idletime, "%4d", d->character->char_specials.timer);
    else
      strcpy(idletime, "");
    
    format = "%-4d %-14s %-15s %-4s %-8s ";
    
    if (d->character && GET_CHAR_NAME(d->character)) {
      if (d->original)
        sprintf(line, format, d->desc_num, GET_CHAR_NAME(d->original),
                state, idletime, timeptr);
      else
        sprintf(line, format, d->desc_num, GET_CHAR_NAME(d->character),
                state, idletime, timeptr);
    } else
      sprintf(line, format, d->desc_num, "UNDEFINED",
              state, idletime, timeptr);
    
    if (*d->host && GET_DESC_LEVEL(d) <= GET_LEVEL(ch))
      sprintf(line + strlen(line), "[%s]\r\n", d->host);
    else
      strcat(line, "[Hostname unknown]\r\n");
    
    if (d->connected) {
      sprintf(line2, "^g%s^n", line);
      strcpy(line, line2);
    }
    if ((d->connected && !d->character) || CAN_SEE(ch, d->character)) {
      send_to_char(line, ch);
      num_can_see++;
    }
  }
  
  sprintf(line, "\r\n%d visible sockets connected.\r\n", num_can_see);
  send_to_char(line, ch);
}

/* Generic page_string function for displaying text */
ACMD(do_gen_ps)
{
  
  switch (subcmd) {
    case SCMD_CREDITS:
      page_string(ch->desc, credits, 0);
      break;
    case SCMD_NEWS:
      page_string(ch->desc, news, 0);
      break;
    case SCMD_INFO:
      page_string(ch->desc, info, 0);
      break;
    case SCMD_IMMLIST:
      send_to_char(ch, immlist);
      break;
    case SCMD_HANDBOOK:
      page_string(ch->desc, handbook, 0);
      break;
    case SCMD_POLICIES:
      page_string(ch->desc, policies, 0);
      break;
    case SCMD_MOTD:
      page_string(ch->desc, motd, 0);
      break;
    case SCMD_IMOTD:
      page_string(ch->desc, imotd, 0);
      break;
    case SCMD_CLEAR:
      send_to_char("\033[H\033[J", ch);
      break;
    case SCMD_VERSION:
      send_to_char(strcat(strcpy(buf, *awakemud_version), "\r\n"), ch);
      break;
    case SCMD_WHOAMI:
      send_to_char(strcat(strcpy(buf, GET_CHAR_NAME(ch)), "\r\n"), ch);
      break;
    default:
      return;
  }
}

extern void nonsensical_reply( struct char_data *ch );

void perform_mortal_where(struct char_data * ch, char *arg)
{
  /* DISABLED FOR MORTALS */
  nonsensical_reply(ch);
  return;
}

void print_object_location(int num, struct obj_data *obj, struct char_data *ch,
                           int recur)
{
  if (strlen(buf) >= 7500)
    return;
  if (num > 0)
    sprintf(buf + strlen(buf), "O%3d. %-25s - ", num,
            GET_OBJ_NAME(obj));
  else
    sprintf(buf + strlen(buf), "%33s", " - ");
  
  if (obj->in_room)
    sprintf(buf + strlen(buf), "[%5ld] %s\r\n", GET_ROOM_VNUM(obj->in_room), GET_ROOM_NAME(obj->in_room));
  else if (obj->carried_by)
    sprintf(buf + strlen(buf), "carried by %s\r\n", PERS(obj->carried_by, ch));
  else if (obj->worn_by)
    sprintf(buf + strlen(buf), "worn by %s\r\n", PERS(obj->worn_by, ch));
  else if (obj->in_obj)
  {
    sprintf(buf + strlen(buf), "inside %s%s\r\n",
            GET_OBJ_NAME(obj->in_obj), (recur ? ", which is" : " "));
    if (recur)
      print_object_location(0, obj->in_obj, ch, recur);
  } else
    sprintf(buf + strlen(buf), "in an unknown location\r\n");
}

void perform_immort_where(struct char_data * ch, char *arg)
{
  struct char_data *i;
  struct descriptor_data *d;
  int num = 0, found = 0;
  int found2 = FALSE;
  
  if (!*arg)
  {
    strcpy(buf, "Players\r\n-------\r\n");
    for (d = descriptor_list; d; d = d->next)
      if (!d->connected) {
        i = (d->original ? d->original : d->character);
        if (i && CAN_SEE(ch, i) && (i->in_room || i->in_veh)) {
          if (d->original)
            if (d->character->in_veh)
              sprintf(buf + strlen(buf), "%-20s - [%5ld] %s^n (switched as %s) (in %s)\r\n",
                      GET_CHAR_NAME(i),
                      GET_ROOM_VNUM(get_ch_in_room(d->character)),
                      GET_ROOM_NAME(get_ch_in_room(d->character)),
                      GET_NAME(d->character),
                      GET_VEH_NAME(d->character->in_veh));
            else
              sprintf(buf + strlen(buf), "%-20s - [%5ld] %s^n (in %s)\r\n",
                      GET_CHAR_NAME(i),
                      GET_ROOM_VNUM(get_ch_in_room(d->character)),
                      GET_ROOM_NAME(get_ch_in_room(d->character)),
                      GET_NAME(d->character));
          
            else
              if (i->in_veh)
                sprintf(buf + strlen(buf), "%-20s - [%5ld] %s^n (in %s)\r\n",
                        GET_CHAR_NAME(i),
                        GET_ROOM_VNUM(get_ch_in_room(i)),
                        GET_ROOM_NAME(get_ch_in_room(i)),
                        GET_VEH_NAME(i->in_veh));
              else
                sprintf(buf + strlen(buf), "%-20s - [%5ld] %s^n\r\n",
                        GET_CHAR_NAME(i),
                        GET_ROOM_VNUM(get_ch_in_room(i)),
                        GET_ROOM_NAME(get_ch_in_room(i)));
        }
      }
    page_string(ch->desc, buf, 1);
  } else
  {
    *buf = '\0';
    for (i = character_list; i; i = i->next)
      if (CAN_SEE(ch, i) && (i->in_room || i->in_veh) &&
          isname(arg, GET_KEYWORDS(i))) {
        found = 1;
        sprintf(buf + strlen(buf), "M%3d. %-25s - [%5ld] %s^n", ++num,
                GET_NAME(i),
                GET_ROOM_VNUM(get_ch_in_room(i)),
                GET_ROOM_NAME(get_ch_in_room(i)));
        if (i->in_veh) {
          sprintf(ENDOF(buf), " (in %s^n)\r\n", GET_VEH_NAME(i->in_veh));
        } else {
          strcat(buf, "\r\n");
        }
      }
    found2 = ObjList.PrintList(ch, arg);
    
    if (!found && !found2)
      send_to_char("Couldn't find any such thing.\r\n", ch);
    else
      page_string(ch->desc, buf, 1);
  }
}

ACMD(do_where)
{
  one_argument(argument, arg);
  
  /* DISABLED FOR MORTALS */
  
  if (GET_REAL_LEVEL(ch) >= LVL_PRESIDENT)
    perform_immort_where(ch, arg);
  else
    perform_mortal_where(ch, arg);
}

ACMD(do_consider)
{
  struct char_data *victim;
  int diff;
  
  one_argument(argument, buf);
  
  if (!(victim = get_char_room_vis(ch, buf))) {
    send_to_char("Consider killing who?\r\n", ch);
    return;
  }
  if (victim == ch) {
    send_to_char("Easy!  Very easy indeed!  \r\n", ch);
    return;
  }
  
  if (IS_NPC(victim)) {
    diff = (GET_BALLISTIC(victim) - GET_BALLISTIC(ch));
    diff += (GET_IMPACT(victim) - GET_IMPACT(ch));
    diff += (GET_BOD(victim) - GET_BOD(ch));
    diff += (GET_QUI(victim) - GET_QUI(ch));
    diff += (GET_STR(victim) - GET_STR(ch));
    diff += (GET_COMBAT(victim) - GET_COMBAT(ch));
    diff += (GET_REA(victim) - GET_REA(ch));
    diff += (GET_INIT_DICE(victim) - GET_INIT_DICE(ch));
    if (GET_MAG(victim) >= 100) {
      diff += (int)((GET_MAG(victim) - GET_MAG(ch)) / 100);
      diff += GET_SKILL(victim, SKILL_SORCERY);
    }
    if (GET_MAG(ch) >= 100 && (IS_NPC(ch) || (GET_TRADITION(ch) == TRAD_HERMETIC ||
                                              GET_TRADITION(ch) == TRAD_SHAMANIC)))
      diff -= GET_SKILL(ch, SKILL_SORCERY);
    if (GET_EQ(victim, WEAR_WIELD))
      diff += GET_SKILL(victim, GET_OBJ_VAL(GET_EQ(victim, WEAR_WIELD), 4));
    else
      diff += GET_SKILL(victim, SKILL_UNARMED_COMBAT);
    if (GET_EQ(ch, WEAR_WIELD))
      diff -= GET_SKILL(ch, GET_OBJ_VAL(GET_EQ(ch, WEAR_WIELD), 4));
    else
      diff -= GET_SKILL(ch, SKILL_UNARMED_COMBAT);
    
    if (diff <= -18)
      send_to_char("Now where did that chicken go?\r\n", ch);
    else if (diff <= -12)
      send_to_char("You could kill it with a needle!\r\n", ch);
    else if (diff <= -6)
      send_to_char("Easy.\r\n", ch);
    else if (diff <= -3)
      send_to_char("Fairly easy.\r\n", ch);
    else if (diff == 0)
      send_to_char("The perfect match!\r\n", ch);
    else if (diff <= 3)
      send_to_char("You would need some luck!\r\n", ch);
    else if (diff <= 6)
      send_to_char("You would need a lot of luck!\r\n", ch);
    else if (diff <= 12)
      send_to_char("You would need a lot of luck and great equipment!\r\n", ch);
    else if (diff <= 18)
      send_to_char("Do you feel lucky, punk?\r\n", ch);
    else if (diff <= 30)
      send_to_char("Are you mad!?\r\n", ch);
    else
      send_to_char("You ARE mad!\r\n", ch);
  } else {
    if (GET_REP(victim) < 26)
      send_to_char("Total greenhorn.\r\n", ch);
    else if (GET_REP(victim) < 50)
      send_to_char("Still finding their feet.\r\n", ch);
    else if (GET_REP(victim) < 101)
      send_to_char("Innocence has been lost.\r\n", ch);
    else if (GET_REP(victim) < 200)
      send_to_char("They can handle themselves.\r\n", ch);
    else if (GET_REP(victim) < 300)
      send_to_char("An accomplished runner.\r\n", ch);
    else if (GET_REP(victim) < 500)
      send_to_char("Definite lifer.\r\n", ch);
    else
      send_to_char("A legend of the Sprawl.\r\n", ch);
  }
}

ACMD(do_diagnose)
{
  struct char_data *vict;
  
  one_argument(argument, buf);
  
  if (*buf) {
    if (!(vict = get_char_room_vis(ch, buf))) {
      send_to_char(NOPERSON, ch);
      return;
    } else
      diag_char_to_char(vict, ch);
  } else {
    if (FIGHTING(ch))
      diag_char_to_char(FIGHTING(ch), ch);
    else
      send_to_char("Diagnose who?\r\n", ch);
  }
}

struct sort_struct
{
  int sort_pos;
  byte is_social;
}
*cmd_sort_info = NULL;

int num_of_cmds;

void sort_commands(void)
{
  int a, b, tmp;
  
  ACMD_DECLARE(do_action);
  
  num_of_cmds = 0;
  
  /*
   * first, count commands (num_of_commands is actually one greater than the
   * number of commands; it inclues the '\n'.
   */
  while (*cmd_info[num_of_cmds].command != '\n')
    num_of_cmds++;
  
  /* create data array */
  cmd_sort_info = new sort_struct[num_of_cmds];
  
  /* initialize it */
  for (a = 1; a < num_of_cmds; a++) {
    cmd_sort_info[a].sort_pos = a;
    cmd_sort_info[a].is_social = (cmd_info[a].command_pointer == do_action);
  }
  
  /* the infernal special case */
  cmd_sort_info[find_command("insult")].is_social = TRUE;
  
  /* Sort.  'a' starts at 1, not 0, to remove 'RESERVED' */
  for (a = 1; a < num_of_cmds - 1; a++)
    for (b = a + 1; b < num_of_cmds; b++)
      if (strcmp(cmd_info[cmd_sort_info[a].sort_pos].command,
                 cmd_info[cmd_sort_info[b].sort_pos].command) > 0) {
        tmp = cmd_sort_info[a].sort_pos;
        cmd_sort_info[a].sort_pos = cmd_sort_info[b].sort_pos;
        cmd_sort_info[b].sort_pos = tmp;
      }
}

ACMD(do_commands)
{
  int no, i, cmd_num;
  int socials = 0;
  struct char_data *vict;
  bool mode_all;
  
  one_argument(argument, arg);
  
  // Note: Providing an argument to COMMANDS used to list the commands available to someone else.
  // This seems rather pointless to me, so I'm changing the behavior to filter based on the prefix you provide. -LS
  
  /*
  if (*arg) {
     
    if (!(vict = get_char_vis(ch, arg)) || IS_NPC(vict)) {
      send_to_char("Who is that?\r\n", ch);
      return;
    }
    if (GET_REAL_LEVEL(ch) < GET_REAL_LEVEL(vict)) {
      send_to_char("You can't see the commands of people above your level.\r\n", ch);
      return;
    }
  } else */
    vict = ch;
  
  if (subcmd == SCMD_SOCIALS)
    socials = 1;
  
  if (!*arg && PRF_FLAGGED(ch, PRF_SCREENREADER)) {
    send_to_char(ch, "The full list of %s is several hundred lines long. We recommend filtering the list by typing %s <prefix>, which will return"
                 " all %s that begin with the specified prefix. If you wish to see all %s, type %s ALL.\r\n",
                 // Here we go...
                 socials ? "socials" : "commands",
                 socials ? "SOCIALS" : "COMMANDS",
                 socials ? "socials" : "commands",
                 socials ? "socials" : "commands",
                 socials ? "SOCIALS" : "COMMANDS"
                 );
    return;
  }
  
  if (*arg && !str_cmp(arg, "all")) {
    sprintf(buf, "The following %s are available to you:\r\n",
            socials ? "socials" : "commands");
    mode_all = TRUE;
  } else {
    sprintf(buf, "The following %s beginning with '%s' are available to you:\r\n",
            socials ? "socials" : "commands", arg);
    mode_all = FALSE;
  }
  
  if (PLR_FLAGGED(ch, PLR_MATRIX)) {
    for (no = 1, cmd_num = 1;;cmd_num++) {
      // Skip any commands that don't match the prefix provided.
      if (!mode_all && *arg && !is_abbrev(arg, mtx_info[cmd_num].command))
        continue;
      if (mtx_info[cmd_num].minimum_level >= 0 &&
          ((!IS_NPC(vict) && GET_REAL_LEVEL(vict) >= mtx_info[cmd_num].minimum_level) ||
           (IS_NPC(vict) && vict->desc->original && GET_REAL_LEVEL(vict->desc->original) >= mtx_info[cmd_num].minimum_level))) {
            sprintf(buf + strlen(buf), "%-13s", mtx_info[cmd_num].command);
            if (!(no % 7) || PRF_FLAGGED(ch, PRF_SCREENREADER))
              strcat(buf, "\r\n");
            no++;
            if (*mtx_info[cmd_num].command == '\n')
              break;
          }
    }
  } else if (PLR_FLAGGED(ch, PLR_REMOTE) || AFF_FLAGGED(ch, AFF_RIG)) {
    for (no = 1, cmd_num = 1;;cmd_num++) {
      // Skip any commands that don't match the prefix provided.
      if (!mode_all && *arg && !is_abbrev(arg, rig_info[cmd_num].command))
        continue;
      
      if (rig_info[cmd_num].minimum_level >= 0 &&
          ((!IS_NPC(vict) && GET_REAL_LEVEL(vict) >= rig_info[cmd_num].minimum_level) ||
           (IS_NPC(vict) && vict->desc->original && GET_REAL_LEVEL(vict->desc->original) >= rig_info[cmd_num].minimum_level))) {
            sprintf(buf + strlen(buf), "%-13s", rig_info[cmd_num].command);
            if (!(no % 7) || PRF_FLAGGED(ch, PRF_SCREENREADER))
              strcat(buf, "\r\n");
            no++;
            if (*rig_info[cmd_num].command == '\n')
              break;
          }
    }
  } else {
    for (no = 1, cmd_num = 1; cmd_num < num_of_cmds; cmd_num++) {
      i = cmd_sort_info[cmd_num].sort_pos;
      
      // Skip any commands that don't match the prefix provided.
      if (!mode_all && *arg && !is_abbrev(arg, cmd_info[i].command))
        continue;
      
      if (cmd_info[i].minimum_level >= 0 &&
          ((!IS_NPC(vict) && GET_REAL_LEVEL(vict) >= cmd_info[i].minimum_level) ||
           (IS_NPC(vict) && vict->desc->original && GET_REAL_LEVEL(vict->desc->original) >= cmd_info[i].minimum_level)) &&
          (socials == cmd_sort_info[i].is_social)) {
        sprintf(buf + strlen(buf), "%-13s", cmd_info[i].command);
        if (!(no % 7) || PRF_FLAGGED(ch, PRF_SCREENREADER))
          strcat(buf, "\r\n");
        no++;
      }
    }
  }
  strcat(buf, "\r\n");
  send_to_char(buf, ch);
}

ACMD(do_scan)
{
  struct char_data *list;
  struct veh_data *veh, *in_veh = NULL;
  bool specific = FALSE, infra, lowlight, onethere, anythere = FALSE, done = FALSE;
  int i = 0, j, dist = 3;
  struct room_data *was_in = NULL, *x = NULL;
  
  if (AFF_FLAGGED(ch, AFF_DETECT_INVIS) && !(PLR_FLAGGED(ch, PLR_REMOTE) || AFF_FLAGGED(ch, AFF_RIG))) {
    send_to_char(ch, "The ultrasound distorts your vision.\r\n");
    return;
  }
  argument = any_one_arg(argument, buf);
  
  if (*buf) {
    if (is_abbrev(buf, "south")) {
      i = SCMD_SOUTH;
      specific = TRUE;
    } else
      for (;!specific && (i < NUM_OF_DIRS); ++i) {
        if (is_abbrev(buf, dirs[i]))
          specific = TRUE;
      }
  }
  if (ch->in_veh || ch->char_specials.rigging) {
    RIG_VEH(ch, in_veh);
    if (ch->in_room)
      x = ch->in_room;
    ch->in_room = in_veh->in_room;
  }
  
  infra = ((PRF_FLAGGED(ch, PRF_HOLYLIGHT) ||
            CURRENT_VISION(ch) == THERMOGRAPHIC) ? TRUE : FALSE);
  lowlight = ((PRF_FLAGGED(ch, PRF_HOLYLIGHT) ||
               CURRENT_VISION(ch) == LOWLIGHT) ? TRUE : FALSE);
  
  if (!infra && IS_ASTRAL(ch))
    infra = TRUE;
  if (!specific) {
    for (i = 0; i < NUM_OF_DIRS; ++i) {
      if (CAN_GO(ch, i)) {
        onethere = FALSE;
        if (!((!infra && light_level(EXIT(ch, i)->to_room) == LIGHT_FULLDARK) ||
              ((!infra || !lowlight) && (light_level(EXIT(ch, i)->to_room) == LIGHT_MINLIGHT || light_level(EXIT(ch, i)->to_room) == LIGHT_PARTLIGHT)))) {
          strcpy(buf1, "");
          for (list = EXIT(ch, i)->to_room->people; list; list = list->next_in_room)
            if (CAN_SEE(ch, list)) {
              if (in_veh) {
                if (in_veh->cspeed > SPEED_IDLE) {
                  if (get_speed(in_veh) >= 200) {
                    if (success_test(GET_INT(ch) + GET_POWER(ch, ADEPT_IMPROVED_PERCEPT), 7) < 1)
                      continue;
                    else if (get_speed(in_veh) < 200 && get_speed(in_veh) >= 120) {
                      if (success_test(GET_INT(ch) + GET_POWER(ch, ADEPT_IMPROVED_PERCEPT), 6) < 1)
                        continue;
                      else if (get_speed(in_veh) < 120 && get_speed(in_veh) >= 60) {
                        if (success_test(GET_INT(ch) + GET_POWER(ch, ADEPT_IMPROVED_PERCEPT), 5) < 1)
                          continue;
                        else
                          if (success_test(GET_INT(ch) + GET_POWER(ch, ADEPT_IMPROVED_PERCEPT), 4) < 1)
                            continue;
                      }
                    }
                  }
                }
                
              }
              sprintf(ENDOF(buf1), "  %s\r\n", GET_NAME(list));
              onethere = TRUE;
              anythere = TRUE;
            }
          for (veh = EXIT(ch, i)->to_room->vehicles; veh; veh = veh->next_veh) {
            if (in_veh) {
              if (in_veh->cspeed > SPEED_IDLE) {
                if (get_speed(in_veh) >= 200) {
                  if (success_test(GET_INT(ch) + GET_POWER(ch, ADEPT_IMPROVED_PERCEPT), 7) < 1)
                    continue;
                  else if (get_speed(in_veh) < 200 && get_speed(in_veh) >= 120) {
                    if (success_test(GET_INT(ch) + GET_POWER(ch, ADEPT_IMPROVED_PERCEPT), 6) < 1)
                      continue;
                    else if (get_speed(in_veh) < 120 && get_speed(in_veh) >= 60) {
                      if (success_test(GET_INT(ch) + GET_POWER(ch, ADEPT_IMPROVED_PERCEPT), 5) < 1)
                        continue;
                      else
                        if (success_test(GET_INT(ch) + GET_POWER(ch, ADEPT_IMPROVED_PERCEPT), 4) < 1)
                          continue;
                    }
                  }
                }
              }
            }
            sprintf(ENDOF(buf1), "  %s (%s)\r\n", GET_VEH_NAME(veh), (get_speed(veh) ? "Moving" : "Stationary"));
            onethere = TRUE;
            anythere = TRUE;
          }
        }
        if (onethere) {
          sprintf(buf2, "%s %s:\r\n%s\r\n", dirs[i], dist_name[0], buf1);
          CAP(buf2);
          send_to_char(buf2, ch);
        }
      }
    }
    if (!anythere) {
      send_to_char("You don't seem to see anyone in the surrounding areas.\r\n", ch);
      if (in_veh) {
        ch->in_room = x;
      }
      return;
    }
  } else {
    --i;
    
    dist = find_sight(ch);
    
    if (CAN_GO(ch, i)) {
      was_in = ch->in_room;
      anythere = FALSE;
      for (j = 0;!done && (j < dist); ++j) {
        onethere = FALSE;
        if (CAN_GO(ch, i)) {
          strcpy(buf1, "");
          for (list = EXIT(ch, i)->to_room->people; list; list = list->next_in_room)
            if (CAN_SEE(ch, list)) {
              if (in_veh) {
                if (in_veh->cspeed > SPEED_IDLE) {
                  if (get_speed(in_veh) >= 200) {
                    if (success_test(GET_INT(ch) + GET_POWER(ch, ADEPT_IMPROVED_PERCEPT), 7) < 1)
                      continue;
                    else if (get_speed(in_veh) < 200 && get_speed(in_veh) >= 120) {
                      if (success_test(GET_INT(ch) + GET_POWER(ch, ADEPT_IMPROVED_PERCEPT), 6) < 1)
                        continue;
                      else if (get_speed(in_veh) < 120 && get_speed(in_veh) >= 60) {
                        if (success_test(GET_INT(ch) + GET_POWER(ch, ADEPT_IMPROVED_PERCEPT), 5) < 1)
                          continue;
                        else
                          if (success_test(GET_INT(ch) + GET_POWER(ch, ADEPT_IMPROVED_PERCEPT), 4) < 1)
                            continue;
                      }
                    }
                  }
                }
              }
              sprintf(ENDOF(buf1), "  %s\r\n", GET_NAME(list));
              onethere = TRUE;
              anythere = TRUE;
            }
          
          ch->in_room = EXIT(ch, i)->to_room;
          
          if (onethere) {
            sprintf(buf2, "%s %s:\r\n%s\r\n", dirs[i], dist_name[j], buf1);
            CAP(buf2);
            send_to_char(buf2, ch);
          }
        } else
          done = TRUE;
      }
      
      ch->in_room = was_in;
      
      if (!anythere) {
        if (in_veh) {
          ch->in_room = x;
        }
        send_to_char("You don't seem to see anyone in that direction.\r\n", ch);
        return;
      }
    } else {
      if (in_veh) {
        ch->in_room = x;
      }
      send_to_char("There is no exit in that direction.\r\n", ch);
      return;
    }
  }
  if (in_veh) {
    ch->in_room = x;
  }
}


ACMD(do_position)
{
  skip_spaces(&argument);
  if (!*argument) {
    send_to_char("Your current position is:\r\n  ", ch);
    list_one_char(ch, ch);
    return;
  }
  if (!strncmp(argument, "clear", strlen(argument))) {
    DELETE_ARRAY_IF_EXTANT(GET_DEFPOS(ch));
    send_to_char(ch, "Position cleared.\r\n");
    return;
  }
  if (GET_POS(ch) == POS_FIGHTING) {
    send_to_char(ch, "You can't set your position while fighting.\r\n");
    return;
  }
  DELETE_ARRAY_IF_EXTANT(GET_DEFPOS(ch));
  GET_DEFPOS(ch) = str_dup(argument);
  send_to_char(ch, "Position set.\r\n");
}

ACMD(do_status)
{
  struct char_data *targ = ch;
  if (GET_LEVEL(ch) >= LVL_BUILDER && *argument) {
    skip_spaces(&argument);
    targ = get_char_room_vis(ch, argument);
    if (!targ)
      targ = get_char_vis(ch, argument);
    if (!targ)
      targ = ch;
  }
  if (ch == targ)
    send_to_char("You are affected by:\r\n", ch);
  else send_to_char(ch, "%s is affected by:\r\n", GET_CHAR_NAME(targ));
  if (ch->real_abils.esshole)
    send_to_char(ch, "  Essence Hole (%.2f)\r\n", (float)targ->real_abils.esshole / 100);
  int total = 0;
  if (GET_TOTALBAL(targ) > GET_QUI(targ))
    total += GET_TOTALBAL(targ) - GET_QUI(targ);
  if (GET_TOTALIMP(targ) > GET_QUI(targ))
    total += GET_TOTALIMP(targ) - GET_QUI(targ);
  if (total > 0) {
    send_to_char("  Bulky Armour (", ch);
    if (total >= GET_QUI(targ))
      send_to_char("Insane)\r\n", ch);
    else if (total >= GET_QUI(targ) * 3/4)
      send_to_char("Serious)\r\n", ch);
    else if (total >= GET_QUI(targ) / 2)
      send_to_char("Moderate)\r\n", ch);
    else
      send_to_char("Light)\r\n", ch);
  }
  if (GET_REACH(targ))
    send_to_char(ch, "Extra Reach (%dm)\r\n", GET_REACH(targ));
  if (GET_DRUG_AFFECT(targ) && GET_DRUG_STAGE(targ) > 0)
    send_to_char(ch, "  %s (%s)\r\n", drug_types[GET_DRUG_AFFECT(targ)].name, GET_DRUG_STAGE(targ) == 1 ? "Up" : "Down");
  for (struct sustain_data *sust = GET_SUSTAINED(targ); sust; sust = sust->next)
    if (!sust->caster) {
      strcpy(buf, spells[sust->spell].name);
      if (sust->spell == SPELL_INCATTR || sust->spell == SPELL_INCCYATTR ||
          sust->spell == SPELL_DECATTR || sust->spell == SPELL_DECCYATTR)
        sprintf(ENDOF(buf), " (%s)", attributes[sust->subtype]);
      send_to_char(buf, ch);
      send_to_char("\r\n", ch);
    }
  if (GET_SUSTAINED_NUM(targ)) {
    send_to_char("You are sustaining:\r\n", ch);
    int i = 1;
    for (struct sustain_data *sust = GET_SUSTAINED(targ); sust; sust = sust->next)
      if (sust->caster || sust->spirit == targ) {
        strcpy(buf, spells[sust->spell].name);
        if (sust->spell == SPELL_INCATTR || sust->spell == SPELL_INCCYATTR ||
            sust->spell == SPELL_DECATTR || sust->spell == SPELL_DECCYATTR)
          sprintf(ENDOF(buf), " (%s)", attributes[sust->subtype]);
        send_to_char(ch, "%d) %s ", i, buf);
        if (sust->focus)
          send_to_char(ch, "(Sustained by %s)", GET_OBJ_NAME(sust->focus));
        if (sust->spirit && sust->spirit != ch)
          send_to_char(ch, "(Sustained by %s)", GET_NAME(sust->spirit));
        send_to_char("\r\n", ch);
        i++;
      }
  }
}

ACMD(do_recap)
{
  if (!GET_QUEST(ch))
    send_to_char(ch, "You're not currently on a run.\r\n");
  else {
    sprintf(buf, "%s told you: \r\n%s", GET_NAME(mob_proto+real_mobile(quest_table[GET_QUEST(ch)].johnson)),
            quest_table[GET_QUEST(ch)].info);
    send_to_char(buf, ch);
  }
}

ACMD(do_mort_show)
{
  struct obj_data *obj = NULL;
  struct char_data *vict = NULL;
  two_arguments(argument, buf1, buf2);
  if (!*buf1 || !*buf2) {
    send_to_char("Show what to who?\r\n", ch);
    return;
  }
  generic_find(buf1, FIND_OBJ_INV | FIND_OBJ_EQUIP, ch, &vict, &obj);
  if (!obj) {
    send_to_char("You don't have that item.\r\n", ch);
    return;
  }
  
  if (!(vict = get_char_room_vis(ch, buf2))) {
    send_to_char("You don't see them here.\r\n", ch);
    return;
  }
  act("$n shows $p to $N.", TRUE, ch, obj, vict, TO_ROOM);
  act("You show $p to $N.", TRUE, ch, obj, vict, TO_CHAR);
  show_obj_to_char(obj, vict, 5);
}

ACMD(do_tke){
  sprintf(buf, "Your current TKE is %d.\r\n", GET_TKE(ch));
  send_to_char(buf, ch);
}
