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
#include "bullet_pants.h"
#include "config.h"

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
extern int calculate_vehicle_entry_load(struct veh_data *veh);

extern int get_weapon_damage_type(struct obj_data* weapon);

extern SPECIAL(trainer);
extern SPECIAL(teacher);
extern SPECIAL(metamagic_teacher);
extern SPECIAL(adept_trainer);
extern SPECIAL(spell_trainer);
extern SPECIAL(johnson);
extern SPECIAL(shop_keeper);
extern SPECIAL(landlord_spec);
extern SPECIAL(fence);
extern SPECIAL(terell_davis);
extern SPECIAL(hacker);
extern SPECIAL(receptionist);
extern SPECIAL(fixer);
extern SPECIAL(mageskill_moore);
extern SPECIAL(mageskill_herbie);
extern SPECIAL(mageskill_hermes);
extern SPECIAL(mageskill_anatoly);
extern SPECIAL(mageskill_nightwing);
extern SPECIAL(marksmanship_first);
extern SPECIAL(marksmanship_second);
extern SPECIAL(marksmanship_third);
extern SPECIAL(marksmanship_fourth);
extern SPECIAL(marksmanship_master);
extern WSPEC(monowhip);

extern bool trainable_attribute_is_maximized(struct char_data *ch, int attribute);
extern float get_bulletpants_weight(struct char_data *ch);
extern bool can_hurt(struct char_data *ch, struct char_data *victim, int attacktype, bool include_func_protections);

#ifdef USE_PRIVATE_CE_WORLD
  extern void display_secret_info_about_object(struct char_data *ch, struct obj_data *obj);
#endif

extern struct teach_data teachers[];

extern const char *pc_readable_extra_bits[];

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

ACMD_DECLARE(do_examine);

/* end blood stuff */

char *make_desc(struct char_data *ch, struct char_data *i, char *buf, int act, bool dont_capitalize_a_an, size_t buf_size)
{
  char buf2[MAX_STRING_LENGTH];
  if (!IS_NPC(i)
      && !IS_SENATOR(ch)
      && ((GET_EQ(i, WEAR_HEAD) && GET_OBJ_VAL(GET_EQ(i, WEAR_HEAD), 7) > 1) 
          || (GET_EQ(i, WEAR_FACE) && GET_OBJ_VAL(GET_EQ(i, WEAR_FACE), 7) > 1)) 
      && (act == 2 
          || success_test(GET_INT(ch) + GET_POWER(ch, ADEPT_IMPROVED_PERCEPT),
                          (GET_EQ(i, WEAR_HEAD) ? GET_OBJ_VAL(GET_EQ(i, WEAR_HEAD), 7) : 0) +
                          (GET_EQ(i, WEAR_FACE) ? GET_OBJ_VAL(GET_EQ(i, WEAR_FACE), 7) : 0)) < 1))
  {
    int conceal = (GET_EQ(i, WEAR_ABOUT) ? GET_OBJ_VAL(GET_EQ(i, WEAR_ABOUT), 7) : 0) +
    (GET_EQ(i, WEAR_BODY) ? GET_OBJ_VAL(GET_EQ(i, WEAR_BODY), 7) : 0) +
    (GET_EQ(i, WEAR_UNDER) ? GET_OBJ_VAL(GET_EQ(i, WEAR_UNDER), 7) : 0);
    int perception_successes = act == 2 ? 4 : success_test(GET_INT(ch) + GET_POWER(ch, ADEPT_IMPROVED_PERCEPT), conceal);
    snprintf(buf, buf_size, "%s", dont_capitalize_a_an ? "a" : "A");
    
    // Size.
    if (perception_successes > 0) {
      if (GET_HEIGHT(i) < 130)
        strlcat(buf, " tiny", buf_size);
      else if (GET_HEIGHT(i) < 160)
        strlcat(buf, " small", buf_size);
      else if (GET_HEIGHT(i) < 190)
        strlcat(buf, "n average", buf_size);
      else if (GET_HEIGHT(i) < 220)
        strlcat(buf, " large", buf_size);
      else
        strlcat(buf, " huge", buf_size);
    }
    
    // Sex.
    if (perception_successes > 2)
      snprintf(ENDOF(buf), buf_size - strlen(buf), " %s", genders[(int)GET_SEX(i)]);
    
    // Race.
    if (perception_successes > 3)
      snprintf(ENDOF(buf), buf_size - strlen(buf), " %s", pc_race_types[(int)GET_RACE(i)]);
    else
      strlcat(buf, " person", buf_size);
      
    if (GET_EQ(i, WEAR_ABOUT))
      snprintf(ENDOF(buf), buf_size - strlen(buf), " wearing %s", decapitalize_a_an(GET_OBJ_NAME(GET_EQ(i, WEAR_ABOUT))));
    else if (GET_EQ(i, WEAR_BODY))
      snprintf(ENDOF(buf), buf_size - strlen(buf), " wearing %s", decapitalize_a_an(GET_OBJ_NAME(GET_EQ(i, WEAR_BODY))));
    else if (GET_EQ(i, WEAR_UNDER))
      snprintf(ENDOF(buf), buf_size - strlen(buf), " wearing %s", decapitalize_a_an(GET_OBJ_NAME(GET_EQ(i, WEAR_UNDER))));
    else
      strlcat(buf, " going nude", buf_size);
  }
  
  else
  {
    struct remem *mem;
    if (!act) {
      strlcpy(buf, dont_capitalize_a_an ? decapitalize_a_an(CAP(GET_NAME(i))) : CAP(GET_NAME(i)), buf_size);
      if (IS_SENATOR(ch) && !IS_NPC(i))
        snprintf(ENDOF(buf), buf_size - strlen(buf), " (%s)", CAP(GET_CHAR_NAME(i)));
      else if ((mem = found_mem(GET_MEMORY(ch), i)))
        snprintf(ENDOF(buf), buf_size - strlen(buf), " (%s)", CAP(mem->mem));
    } else if ((mem = found_mem(GET_MEMORY(ch), i)) && act != 2)
      strlcpy(buf, CAP(mem->mem), buf_size);
    else
      strlcpy(buf, dont_capitalize_a_an ? decapitalize_a_an(CAP(GET_NAME(i))) : CAP(GET_NAME(i)), buf_size);
  }
  if (GET_SUSTAINED(i) && (IS_ASTRAL(ch) || IS_DUAL(ch)))
  {
    for (struct sustain_data *sust = GET_SUSTAINED(i); sust; sust = sust->next)
      if (!sust->caster) {
        strlcat(buf, ", surrounded by a spell aura", buf_size);
        break;
      }
  }
  
  if (!IS_NPC(i) && PRF_FLAGGED(ch, PRF_LONGWEAPON) && GET_EQ(i, WEAR_WIELD))
    snprintf(ENDOF(buf), buf_size - strlen(buf), ", wielding %s", decapitalize_a_an(GET_OBJ_NAME(GET_EQ(i, WEAR_WIELD))));
  
  if (AFF_FLAGGED(i, AFF_MANIFEST) && !(IS_ASTRAL(ch) || IS_DUAL(ch)))
  {
    snprintf(buf2, sizeof(buf2), "The ghostly image of %s", buf);
    strlcpy(buf, buf2, buf_size);
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
  snprintf(buf2, sizeof(buf2), "%s is ", CAP(GET_OBJ_NAME(obj)));
  if (condition >= 100)
    strlcat(buf2, "in excellent condition.", sizeof(buf2));
  else if (condition >= 90)
    strlcat(buf2, "barely damaged.", sizeof(buf2));
  else if (condition >= 80)
    strlcat(buf2, "lightly damaged.", sizeof(buf2));
  else if (condition >= 60)
    strlcat(buf2, "moderately damaged.", sizeof(buf2));
  else if (condition >= 30)
    strlcat(buf2, "seriously damaged.", sizeof(buf2));
  else if (condition >= 10)
    strlcat(buf2, "extremely damaged.", sizeof(buf2));
  else
    strlcat(buf2, "almost completely destroyed.", sizeof(buf2));
  strlcat(buf2, "\r\n", sizeof(buf2));
  send_to_char(buf2, ch);
}

void show_obj_to_char(struct obj_data * object, struct char_data * ch, int mode)
{
  *buf = '\0';
  if ((mode == 0) && object->text.room_desc)
  {
    strlcpy(buf, CCHAR ? CCHAR : "", sizeof(buf));
    if (object->graffiti)
      strlcat(buf, object->graffiti, sizeof(buf));
    else {
      // Gun magazines get special consideration.
      if (GET_OBJ_TYPE(object) == ITEM_GUN_MAGAZINE && GET_MAGAZINE_BONDED_MAXAMMO(object)) {
        snprintf(buf, sizeof(buf), "^gA%s %d-round %s %s magazine has been left here.^n",
          GET_MAGAZINE_AMMO_COUNT(object) <= 0 ? "n empty" : "",
          GET_MAGAZINE_BONDED_MAXAMMO(object),
          ammo_type[GET_MAGAZINE_AMMO_TYPE(object)].name,
          weapon_type[GET_MAGAZINE_BONDED_ATTACKTYPE(object)]
        );
      // As do ammo boxes.
      } else if (GET_OBJ_TYPE(object) == ITEM_GUN_AMMO) {
        snprintf(buf, sizeof(buf), "^gA metal box of %s has been left here.^n", get_ammo_representation(GET_AMMOBOX_WEAPON(object), GET_AMMOBOX_TYPE(object), 0));
      } else {
        strlcat(buf, object->text.room_desc, sizeof(buf));
      }
    }
  } else if (GET_OBJ_NAME(object) && mode == 1)
  {
    strlcpy(buf, GET_OBJ_NAME(object), sizeof(buf));
    if (GET_OBJ_TYPE(object) == ITEM_DESIGN)
      strlcat(buf, " (Plan)", sizeof(buf));
    if (GET_OBJ_VNUM(object) == 108 && !GET_OBJ_TIMER(object))
      strlcat(buf, " (Uncooked)", sizeof(buf));
    if (GET_OBJ_TYPE(object) == ITEM_FOCUS && GET_OBJ_VAL(object, 9) == GET_IDNUM(ch))
      strlcat(buf, " ^Y(Geas)^n", sizeof(buf));
  } else if (GET_OBJ_NAME(object) && ((mode == 2) || (mode == 3) || (mode == 4) || (mode == 7)))
    strlcpy(buf, GET_OBJ_NAME(object), sizeof(buf));
  else if (mode == 5)
  {
    if (GET_OBJ_DESC(object))
      strlcpy(buf, GET_OBJ_DESC(object), sizeof(buf));
    else
      strlcpy(buf, "You see nothing special..", sizeof(buf));
  } else if (mode == 8)
    snprintf(buf, sizeof(buf), "\t\t\t\t%s", GET_OBJ_NAME(object));
  if (mode == 7 || mode == 8) {
    if (GET_OBJ_TYPE(object) == ITEM_HOLSTER)
    {
      if (object->contains)
        snprintf(ENDOF(buf), sizeof(buf) - strlen(buf), " (Holding %s)", GET_OBJ_NAME(object->contains));
      if (GET_OBJ_VAL(object, 3) == 1 && ((object->worn_by && object->worn_by == ch) ||
                                          (object->in_obj && object->in_obj->worn_by && object->in_obj->worn_by == ch)))
        strlcat(buf, " ^Y(Ready)", sizeof(buf));
    } else if (GET_OBJ_TYPE(object) == ITEM_WORN && object->contains && !PRF_FLAGGED(ch, PRF_COMPACT))
      strlcat(buf, " carrying:", sizeof(buf));
    else if (GET_OBJ_TYPE(object) == ITEM_FOCUS) {
      if (GET_OBJ_VAL(object, 4))
        strlcat(buf, " ^m(Activated Focus)^n", sizeof(buf));
      if (GET_OBJ_VAL(object, 9) == GET_IDNUM(ch))
        strlcat(buf, " ^Y(Geas)^n", sizeof(buf));
    }
    
    if (GET_OBJ_CONDITION(object) * 100 / MAX(1, GET_OBJ_BARRIER(object)) < 100)
      strlcat(buf, " (damaged)", sizeof(buf));
  }
  if (mode != 3)
  {
    if (IS_OBJ_STAT(object, ITEM_INVISIBLE)) {
      strlcat(buf, " ^B(invisible)", sizeof(buf));
    }
    
    if ((GET_OBJ_TYPE(object) == ITEM_FOCUS || IS_OBJ_STAT(object, ITEM_MAGIC))
        && (IS_ASTRAL(ch) || IS_DUAL(ch))) {
      strlcat(buf, " ^Y(magic aura)", sizeof(buf));
    }
    
    if (IS_OBJ_STAT(object, ITEM_GLOW)) {
      strlcat(buf, " ^W(glowing)", sizeof(buf));
    }
    
    if (IS_OBJ_STAT(object, ITEM_HUM)) {
      strlcat(buf, " ^c(humming)", sizeof(buf));
    }
    
    if (object->obj_flags.quest_id && object->obj_flags.quest_id == GET_IDNUM(ch)) {
      strlcat(buf, " ^Y(Yours)^n", sizeof(buf));
    }
  }
  strlcat(buf, "^N\r\n", sizeof(buf));
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
  
  strlcpy(buf, CCHAR ? CCHAR : "", sizeof(buf));
  
  if (vehicle->damage >= VEH_DAM_THRESHOLD_DESTROYED)
  {
    snprintf(ENDOF(buf), sizeof(buf) - strlen(buf), "%s lies here wrecked.", GET_VEH_NAME(vehicle));
  } 
  
  else {
    if (vehicle->type == VEH_BIKE && vehicle->people && CAN_SEE(ch, vehicle->people))
      snprintf(ENDOF(buf), sizeof(buf) - strlen(buf), "%s sitting on ", CAP(found_mem(GET_MEMORY(ch), vehicle->people) ?
                                                found_mem(GET_MEMORY(ch), vehicle->people)->mem :
                                                GET_NAME(vehicle->people)));
    switch (vehicle->cspeed) {
      case SPEED_OFF:
        if ((vehicle->type == VEH_BIKE && vehicle->people) || vehicle->restring)
          snprintf(ENDOF(buf), sizeof(buf) - strlen(buf), "%s waits here", GET_VEH_NAME(vehicle));
        else
          strlcat(buf, vehicle->description, sizeof(buf));
        break;
      case SPEED_IDLE:
        snprintf(ENDOF(buf), sizeof(buf) - strlen(buf), "%s idles here", GET_VEH_NAME(vehicle));
        break;
      case SPEED_CRUISING:
        snprintf(ENDOF(buf), sizeof(buf) - strlen(buf), "%s cruises through here", GET_VEH_NAME(vehicle));
        break;
      case SPEED_SPEEDING:
        snprintf(ENDOF(buf), sizeof(buf) - strlen(buf), "%s speeds past you", GET_VEH_NAME(vehicle));
        break;
      case SPEED_MAX:
        snprintf(ENDOF(buf), sizeof(buf) - strlen(buf), "%s zooms by you", GET_VEH_NAME(vehicle));
        break;
    }
    if (vehicle->towing) {
      snprintf(ENDOF(buf), sizeof(buf) - strlen(buf), ", towing %s", GET_VEH_NAME(vehicle->towing));
    }
    strlcat(buf, ".", sizeof(buf));
  }
  
  if (vehicle->owner && GET_IDNUM(ch) == vehicle->owner)
    strlcat(buf, " ^Y(Yours)", sizeof(buf));
  strlcat(buf, "^N\r\n", sizeof(buf));
  send_to_char(buf, ch);
}

void list_veh_to_char(struct veh_data * list, struct char_data * ch)
{
  struct veh_data *i;
  for (i = list; i; i = i->next_veh) {
    if (ch->in_veh != i && ch->char_specials.rigging != i)
      show_veh_to_char(i, ch);
    
    if (i == i->next_veh) {
      char errbuf[1000];
      snprintf(errbuf, sizeof(errbuf), "SYSERR: Infinite loop in list_veh_to_char for %s (%ld) at %s (%ld). Breaking the list.",
               GET_VEH_NAME(i),
               i->veh_number,
               GET_ROOM_NAME(get_veh_in_room(i)),
               GET_ROOM_VNUM(get_veh_in_room(i)));
      i->next_veh = NULL;
      mudlog(errbuf, ch, LOG_SYSLOG, TRUE);
      break;
    }
  }
}

#define IS_INVIS(o) IS_OBJ_STAT(o, ITEM_INVISIBLE)

bool items_are_visually_similar(struct obj_data *first, struct obj_data *second) {
  if (!first || !second) {
    mudlog("SYSERR: Received null object to items_are_visually_similar.", NULL, LOG_SYSLOG, TRUE);
    return FALSE;
  }
  
  // Biggest litmus test: Are they even the same thing?
  if (first->item_number != second->item_number)
    return FALSE;
    
  // Quest items don't visually stack.
  if (first->obj_flags.quest_id != second->obj_flags.quest_id)
    return FALSE;
    
  // If the names don't match, they're not similar.
  if (strcmp(first->text.name, second->text.name))
    return FALSE;
    
  // If their invis statuses don't match...
  if (IS_INVIS(first) != IS_INVIS(second))
    return FALSE;
    
  // If their restrings don't match...
  if ((first->restring && !second->restring) || 
      (!first->restring && second->restring) ||
      (first->restring && second->restring && strcmp(first->restring, second->restring)))
    return FALSE;
    
  // If they're magazines, their various bonded stats must match too.
  if (GET_OBJ_TYPE(first) == ITEM_GUN_MAGAZINE) {
    if (GET_MAGAZINE_BONDED_MAXAMMO(first) != GET_MAGAZINE_BONDED_MAXAMMO(second) ||
        GET_MAGAZINE_BONDED_ATTACKTYPE(first) != GET_MAGAZINE_BONDED_ATTACKTYPE(second))
      return FALSE;
  }
  
  return TRUE;
}

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
      
    if (ch->char_specials.rigging) {
      if (ch->char_specials.rigging->cspeed > SPEED_IDLE) {
        int success_test_tn;
        
        if (get_speed(ch->char_specials.rigging) >= 240) {
          success_test_tn = 6;
        } else if (get_speed(ch->char_specials.rigging) < 240 && get_speed(ch->char_specials.rigging) >= 180) {
          success_test_tn = 5;
        } else if (get_speed(ch->char_specials.rigging) < 180 && get_speed(ch->char_specials.rigging) >= 90) {
          success_test_tn = 4;
        } else {
          success_test_tn = 3;
        }
        
        if (success_test(GET_INT(ch) + GET_POWER(ch, ADEPT_IMPROVED_PERCEPT), success_test_tn) <= 0)
          continue;
      }
    } else if (ch->in_veh && i->in_room && i->in_room == ch->in_veh->in_room) {
      if (ch->in_veh->cspeed > SPEED_IDLE) {
        int success_test_tn;
        
        if (get_speed(ch->in_veh) >= 200) {
          success_test_tn = 7;
        } else if (get_speed(ch->in_veh) >= 120) {
          success_test_tn = 6;
        } else if (get_speed(ch->in_veh) >= 60) {
          success_test_tn = 5;
        } else {
          success_test_tn = 4;
        }
        
        if (success_test(GET_INT(ch) + GET_POWER(ch, ADEPT_IMPROVED_PERCEPT), success_test_tn) <= 0)
          continue;
      }
    }
    
    while (i->next_content) {
      if (!items_are_visually_similar(i, i->next_content))
        break;
        
      if (CAN_SEE_OBJ(ch, i))
        num++;
        
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
  
  make_desc(ch, i, buf, TRUE, FALSE, sizeof(buf));
  CAP(buf);
  
  if (phys >= 100 || (GET_TRADITION(i) == TRAD_ADEPT && phys >= 0 &&
                      ((100 - phys) / 10) <= GET_POWER(i, ADEPT_PAIN_RESISTANCE)))
    strlcat(buf, " is in excellent physical condition", sizeof(buf));
  else if (phys >= 90)
    strlcat(buf, " has a few scratches", sizeof(buf));
  else if (phys >= 75)
    strlcat(buf, " has some small wounds and bruises", sizeof(buf));
  else if (phys >= 50)
    strlcat(buf, " has quite a few wounds", sizeof(buf));
  else if (phys >= 30)
    strlcat(buf, " has some big nasty wounds and scratches", sizeof(buf));
  else if (phys >= 15)
    strlcat(buf, " looks pretty hurt", sizeof(buf));
  else if (phys >= 0)
    strlcat(buf, " is in awful condition", sizeof(buf));
  else
    strlcat(buf, " is bleeding awfully from big wounds", sizeof(buf));
  
  if (phys <= 0)
    strlcat(buf, " and is unconscious.\r\n", sizeof(buf));
  else if (ment >= 100 || (GET_TRADITION(i) == TRAD_ADEPT && ment >= 0 &&
                           ((100 - ment) / 10) <= (GET_POWER(i, ADEPT_PAIN_RESISTANCE) -
                                                   (int)((GET_MAX_PHYSICAL(i) - GET_PHYSICAL(i)) / 100))))
    strlcat(buf, " and is alert.\r\n", sizeof(buf));
  else if (ment >= 90)
    strlcat(buf, " and is barely tired.\r\n", sizeof(buf));
  else if (ment >= 75)
    strlcat(buf, " and is slightly worn out.\r\n", sizeof(buf));
  else if (ment >= 50)
    strlcat(buf, " and is fatigued.\r\n", sizeof(buf));
  else if (ment >= 30)
    strlcat(buf, " and is weary.\r\n", sizeof(buf));
  else if (ment >= 10)
    strlcat(buf, " and is groggy.\r\n", sizeof(buf));
  else
    strlcat(buf, " and is completely unconscious.\r\n", sizeof(buf));
  
  
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
        snprintf(buf, sizeof(buf), "$e look%s to be about %0.1f meters tall and "
                "appears to weigh about %d kg.", HSSH_SHOULD_PLURAL(i) ? "s" : "", height, weight);
        act(buf, FALSE, i, 0, ch, TO_VICT);
      } else {
        snprintf(buf, sizeof(buf), "$e look%s to be about %0.1f meters tall and "
                "appears to barely weigh anything.", HSSH_SHOULD_PLURAL(i) ? "s" : "", height);
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
          snprintf(buf, sizeof(buf), "%s aura is ", CAP(HSHR(i)));
          if (GET_GRADE(i) > 6)
            strlcat(buf, "^Wblindingly bright^n.", sizeof(buf));
          else if (GET_GRADE(i) > 2)
            strlcat(buf, "^Wshining bright^n.", sizeof(buf));
          else strlcat(buf, "^Wsomewhat bright^n.", sizeof(buf));
          strlcat(buf, "\r\n", sizeof(buf));
          send_to_char(buf, ch);
        }
      }
      if (IS_DUAL(i) && dual)
        send_to_char(ch, "%s %s dual.\r\n", CAP(HSSH(i)), ISARE(i));
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
          snprintf(ENDOF(buf), sizeof(buf) - strlen(buf), "%s\r\n", GET_OBJ_NAME(tmp_obj));
        } else snprintf(ENDOF(buf2), sizeof(buf2) - strlen(buf2), "%s\r\n", cyber_types[GET_OBJ_VAL(tmp_obj, 0)]);
        break;
      case CYB_TORSO:
      case CYB_SKULL:
        if (GET_OBJ_VAL(tmp_obj, 3)) {
          found = TRUE;
          snprintf(ENDOF(buf), sizeof(buf) - strlen(buf), "%s\r\n", GET_OBJ_NAME(tmp_obj));
        } else snprintf(ENDOF(buf2), sizeof(buf2) - strlen(buf2), "%s\r\n", cyber_types[GET_OBJ_VAL(tmp_obj, 0)]);
        break;
      case CYB_DATAJACK:
        if (GET_EQ(i, WEAR_HEAD))
          continue;
        if (GET_OBJ_VAL(tmp_obj, 3)) {
          found = TRUE;
          snprintf(ENDOF(buf), sizeof(buf) - strlen(buf), "%s\r\n", GET_OBJ_NAME(tmp_obj));
        } else snprintf(ENDOF(buf2), sizeof(buf2) - strlen(buf2), "%s\r\n", cyber_types[GET_OBJ_VAL(tmp_obj, 0)]);
        break;
      case CYB_CHIPJACK:
        if (GET_EQ(i, WEAR_HEAD))
          continue;
        // fall through.
      case CYB_DERMALPLATING:
      case CYB_BALANCETAIL:
        snprintf(ENDOF(buf2), sizeof(buf2) - strlen(buf2), "%s\r\n", cyber_types[GET_OBJ_VAL(tmp_obj, 0)]);
        break;
      case CYB_EYES:
        if (GET_EQ(i, WEAR_EYES) || (GET_EQ(i, WEAR_HEAD) && GET_OBJ_VAL(GET_EQ(i, WEAR_HEAD), 7) > 1))
          continue;
        if ((IS_SET(GET_OBJ_VAL(tmp_obj, 3), EYE_OPTMAG1) || IS_SET(GET_OBJ_VAL(tmp_obj, 3), EYE_OPTMAG2) ||
             IS_SET(GET_OBJ_VAL(tmp_obj, 3), EYE_OPTMAG3)) && success_test(GET_INT(ch), 9) > 0)
          strlcat(buf2, "Optical Magnification\r\n", sizeof(buf2));
        if (IS_SET(GET_OBJ_VAL(tmp_obj, 3), EYE_COSMETIC))
          snprintf(ENDOF(buf2), sizeof(buf2) - strlen(buf2), "%s\r\n", GET_OBJ_NAME(tmp_obj));
        break;
      default:
        found = TRUE;
        snprintf(ENDOF(buf), sizeof(buf) - strlen(buf), "%s\r\n", GET_OBJ_NAME(tmp_obj));
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
      strlcpy(buf, "*", sizeof(buf));
    else
      *buf = '\0';
    
    // Note quest or nokill protection.
    if (i->mob_specials.quest_id) {
      if (i->mob_specials.quest_id == GET_IDNUM(ch)) {
        strlcat(buf, "^Y(Quest)^n ", sizeof(buf));
      }
    } else if (
      (i->in_room && (GET_ROOM_SPEC(i->in_room) == mageskill_moore || GET_ROOM_SPEC(i->in_room) == mageskill_hermes))
      || mob_index[GET_MOB_RNUM(i)].func == mageskill_herbie
      || mob_index[GET_MOB_RNUM(i)].sfunc == mageskill_herbie
      || mob_index[GET_MOB_RNUM(i)].func == mageskill_anatoly
      || mob_index[GET_MOB_RNUM(i)].sfunc == mageskill_anatoly
      || mob_index[GET_MOB_RNUM(i)].func == mageskill_nightwing
      || mob_index[GET_MOB_RNUM(i)].sfunc == mageskill_nightwing
    ) {
      for (struct obj_data *recom = ch->carrying; recom; recom = recom->next_content) {
        if (GET_OBJ_VNUM(recom) == OBJ_MAGE_LETTER) {
          strlcat(buf, "^Y(Skill Quest)^n ", sizeof(buf));
          break;
        }
      }
    } 
#ifdef USE_PRIVATE_CE_WORLD
    else if (
      mob_index[GET_MOB_RNUM(i)].func == marksmanship_first
      || mob_index[GET_MOB_RNUM(i)].sfunc == marksmanship_first
      || mob_index[GET_MOB_RNUM(i)].func == marksmanship_second
      || mob_index[GET_MOB_RNUM(i)].sfunc == marksmanship_second
      || mob_index[GET_MOB_RNUM(i)].func == marksmanship_third
      || mob_index[GET_MOB_RNUM(i)].sfunc == marksmanship_third
      || mob_index[GET_MOB_RNUM(i)].func == marksmanship_fourth
      || mob_index[GET_MOB_RNUM(i)].sfunc == marksmanship_fourth
      || mob_index[GET_MOB_RNUM(i)].func == marksmanship_master
      || mob_index[GET_MOB_RNUM(i)].sfunc == marksmanship_master
    ) {
      for (struct obj_data *recom = ch->carrying; recom; recom = recom->next_content) {
        if (GET_OBJ_VNUM(recom) == OBJ_MARKSMAN_LETTER) {
          strlcat(buf, "^Y(Skill Quest)^n ", sizeof(buf));
          break;
        }
      }
    }
#endif
    
    if (IS_ASTRAL(ch) || IS_DUAL(ch)) {
      if (IS_ASTRAL(i))
        strlcat(buf, "(astral) ", sizeof(buf));
      else if (IS_DUAL(i) && IS_NPC(i))
        strlcat(buf, "(dual) ", sizeof(buf));
    }
    if (AFF_FLAGGED(i, AFF_MANIFEST) && !(IS_ASTRAL(ch) || IS_DUAL(ch)))
      strlcat(buf, "The ghostly image of ", sizeof(buf));
    strlcat(buf, i->player.physical_text.room_desc, sizeof(buf));
    
#define MOB_HAS_SPEC(rnum, spec) (mob_index[(rnum)].func == spec || mob_index[(rnum)].sfunc == spec)

    if (DISPLAY_HELPFUL_STRINGS_FOR_MOB_FUNCS) {
      bool already_printed = FALSE;
      if (mob_index[GET_MOB_RNUM(i)].func || mob_index[GET_MOB_RNUM(i)].sfunc) {
        if (MOB_HAS_SPEC(GET_MOB_RNUM(i), trainer)) {
          snprintf(ENDOF(buf), sizeof(buf) - strlen(buf), "^y...%s%s %s willing to train you.%s^n\r\n", 
                   HSSH(i), 
                   already_printed ? " also" : "",
                   HSSH_SHOULD_PLURAL(i) ? "looks" : "look",
                   GET_TKE(ch) < NEWBIE_KARMA_THRESHOLD ? " Use the ^YTRAIN^y command to begin." : "");
          already_printed = TRUE;
        }
        if (MOB_HAS_SPEC(GET_MOB_RNUM(i), teacher)) {
          if (MOB_FLAGGED(i, MOB_INANIMATE)) {
            snprintf(ENDOF(buf), sizeof(buf) - strlen(buf), "^y...it%s can be used to help you practice your skills.%s^n\r\n",
                     already_printed ? " also" : "",
                     GET_TKE(ch) < NEWBIE_KARMA_THRESHOLD ? " Use the ^YPRACTICE^y command to begin." : "");
          } else {
            snprintf(ENDOF(buf), sizeof(buf) - strlen(buf), "^y...%s%s %s willing to help you practice your skills.%s^n\r\n", 
                     HSSH(i), 
                     already_printed ? " also" : "",
                     HSSH_SHOULD_PLURAL(i) ? "looks" : "look",
                     GET_TKE(ch) < NEWBIE_KARMA_THRESHOLD ? " Use the ^YPRACTICE^y command to begin." : "");
          }
          already_printed = TRUE;
        }
        if (MOB_HAS_SPEC(GET_MOB_RNUM(i), metamagic_teacher)) {
          // Mundanes can't see metamagic teachers' abilities.
          if (GET_TRADITION(ch) != TRAD_MUNDANE) {
            snprintf(ENDOF(buf), sizeof(buf) - strlen(buf), "^y...%s%s %s willing to help you train in metamagic techniques.%s^n\r\n", 
                     HSSH(i), 
                     already_printed ? " also" : "",
                     HSSH_SHOULD_PLURAL(i) ? "looks" : "look",
                     GET_TKE(ch) < NEWBIE_KARMA_THRESHOLD ? " Use the ^YTRAIN^y command to begin." : "");
            already_printed = TRUE;
          }
        }
        if (MOB_HAS_SPEC(GET_MOB_RNUM(i), adept_trainer)) {
          // Adepts can't see adept trainers' abilities.
          if (GET_TRADITION(ch) == TRAD_ADEPT) {
            snprintf(ENDOF(buf), sizeof(buf) - strlen(buf), "^y...%s%s %s willing to help you train your powers.%s^n\r\n", 
                     HSSH(i), 
                     already_printed ? " also" : "",
                     HSSH_SHOULD_PLURAL(i) ? "looks" : "look",
                     GET_TKE(ch) < NEWBIE_KARMA_THRESHOLD ? " Use the ^YTRAIN^y command to begin." : "");
            already_printed = TRUE;
          }
        }
        if (MOB_HAS_SPEC(GET_MOB_RNUM(i), spell_trainer)) {
          // Mundanes and adepts can't see spell trainers' abilities.
          if (GET_TRADITION(ch) != TRAD_MUNDANE && GET_TRADITION(ch) != TRAD_ADEPT) {
            snprintf(ENDOF(buf), sizeof(buf) - strlen(buf), "^y...%s%s %s willing to help you learn new spells.%s^n\r\n", 
                     HSSH(i), 
                     already_printed ? " also" : "",
                     HSSH_SHOULD_PLURAL(i) ? "looks" : "look",
                     GET_TKE(ch) < NEWBIE_KARMA_THRESHOLD ? " Use the ^YLEARN^y command to begin." : "");
            already_printed = TRUE;
          }
        }
        if (MOB_HAS_SPEC(GET_MOB_RNUM(i), johnson)) {
          snprintf(ENDOF(buf), sizeof(buf) - strlen(buf), "^y...%s%s might have a job for you.%s^n\r\n", 
                   HSSH(i), 
                   already_printed ? " also" : "",
                   GET_TKE(ch) < NEWBIE_KARMA_THRESHOLD ? " See ^YHELP JOB^y for instructions." : "");
          already_printed = TRUE;
        }
        if (MOB_HAS_SPEC(GET_MOB_RNUM(i), shop_keeper) || MOB_HAS_SPEC(GET_MOB_RNUM(i), terell_davis)) {
          snprintf(ENDOF(buf), sizeof(buf) - strlen(buf), "^y...%s%s %s a few things for sale.%s^n\r\n", 
                   HSSH(i), 
                   already_printed ? " also" : "", 
                   HASHAVE(i),
                   GET_TKE(ch) < NEWBIE_KARMA_THRESHOLD ? " Use the ^YLIST^y command to see them." : "");
          already_printed = TRUE;
        }
        if (MOB_HAS_SPEC(GET_MOB_RNUM(i), landlord_spec)) {
          snprintf(ENDOF(buf), sizeof(buf) - strlen(buf), "^y...%s%s might have some rooms for lease.%s^n\r\n", 
                   HSSH(i), 
                   already_printed ? " also" : "",
                   GET_TKE(ch) < NEWBIE_KARMA_THRESHOLD ? " See ^YHELP APARTMENTS^y for details." : "");
          already_printed = TRUE;
        }
        if (MOB_HAS_SPEC(GET_MOB_RNUM(i), fence)) {
          snprintf(ENDOF(buf), sizeof(buf) - strlen(buf), "^y...%s%s might be willing to buy paydata from you.%s^n\r\n", 
                   HSSH(i), 
                   already_printed ? " also" : "",
                   GET_TKE(ch) < NEWBIE_KARMA_THRESHOLD ? " If you have paydata, ^YUNINSTALL^y it from your deck and then ^YSELL PAYDATA^y." : "");
          already_printed = TRUE;
        }
        if (MOB_HAS_SPEC(GET_MOB_RNUM(i), hacker)) {
          snprintf(ENDOF(buf), sizeof(buf) - strlen(buf), "^y...%s%s cracks credsticks-- you can ^YGIVE^y them to %s.^n\r\n", 
                   HSSH(i), 
                   already_printed ? " also" : "", 
                   HMHR(i));
          already_printed = TRUE;
        }
        if (MOB_HAS_SPEC(GET_MOB_RNUM(i), receptionist)) {
          snprintf(ENDOF(buf), sizeof(buf) - strlen(buf), "^y...%s%s %s bunks for rent.%s^n\r\n", 
                   HSSH(i), 
                   already_printed ? " also" : "", 
                   HASHAVE(i),
                   GET_TKE(ch) < NEWBIE_KARMA_THRESHOLD ? " Renting is purely for flavor and is not required in AwakeMUD." : "");
          already_printed = TRUE;
        }
        if (MOB_HAS_SPEC(GET_MOB_RNUM(i), fixer)) {
          snprintf(ENDOF(buf), sizeof(buf) - strlen(buf), "^y...%s%s can repair objects for you.%s^n\r\n",
                   HSSH(i), 
                   already_printed ? " also" : "",
                   GET_TKE(ch) < NEWBIE_KARMA_THRESHOLD ? " Use the ^YREPAIR^y command to proceed." : "");
          already_printed = TRUE;
        }
        if (i->in_room && GET_ROOM_SPEC(i->in_room) == mageskill_hermes) {
          for (int qidx = 0; qidx <= QUEST_TIMER; qidx++) {
            if (GET_LQUEST(ch, qidx) == QST_MAGE_INTRO) {
              snprintf(ENDOF(buf), sizeof(buf) - strlen(buf), "^y...%s is%s wearing some sweet bling. Harold said to ^YASK^y %s about %s chain.^n\r\n",
                       HSSH(i),
                       already_printed ? " also" : "",
                       HMHR(i),
                       HSHR(i));
              already_printed = TRUE;
              break;
            }
          }
        }
        
#ifdef USE_PRIVATE_CE_WORLD
        if (MOB_HAS_SPEC(GET_MOB_RNUM(i), marksmanship_master)) {
          if (SHOTS_FIRED(ch) >= MARKSMAN_QUEST_SHOTS_FIRED_REQUIREMENT && SHOTS_TRIGGERED(ch) != -1) {
            snprintf(ENDOF(buf), sizeof(buf) - strlen(buf), "^y...%s%s looks like %s can teach you a few things. You should ^YASK^y %s about getting some training.^n\r\n",
                     HSSH(i),
                     already_printed ? " also" : "",
                     HSSH(i),
                     HMHR(i));
            already_printed = TRUE;
          }
        }
#endif
      }
    }
    
    send_to_char(buf, ch);
    
    return;
  }
  make_desc(ch, i, buf, FALSE, FALSE, sizeof(buf));
  if (PRF_FLAGGED(i, PRF_AFK))
    strlcat(buf, " (AFK)", sizeof(buf));
  if (PLR_FLAGGED(i, PLR_SWITCHED))
    strlcat(buf, " (switched)", sizeof(buf));
  if (IS_AFFECTED(i, AFF_INVISIBLE) || IS_AFFECTED(i, AFF_IMP_INVIS) || IS_AFFECTED(i, AFF_SPELLINVIS) || IS_AFFECTED(i, AFF_SPELLIMPINVIS))
    strlcat(buf, " (invisible)", sizeof(buf));
  if (PLR_FLAGGED(ch, PLR_NEWBIE) && !IS_NPC(i))
    strlcat(buf, " (player)", sizeof(buf));
  if (IS_AFFECTED(i, AFF_HIDE))
    strlcat(buf, " (hidden)", sizeof(buf));
  if (!IS_NPC(i) && !i->desc &&
      !PLR_FLAGS(i).AreAnySet(PLR_MATRIX, PLR_PROJECT, PLR_SWITCHED, ENDBIT))
    strlcat(buf, " (linkless)", sizeof(buf));
  if (IS_ASTRAL(ch) || IS_DUAL(ch)) {
    bool dual = TRUE;
    if (IS_ASTRAL(i))
      strlcat(buf, " (astral)", sizeof(buf));
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
          strlcat(buf, " ^W(Blinding Aura)^n", sizeof(buf));
        else if (GET_GRADE(i) > 2)
          strlcat(buf, " ^W(Shining Aura)^n", sizeof(buf));
        else strlcat(buf, " ^W(Bright Aura)^n", sizeof(buf));
      }
    }
    if (IS_DUAL(i) && dual)
      strlcat(buf, " (dual)", sizeof(buf));
  }
  if (PLR_FLAGGED(i, PLR_WRITING))
    strlcat(buf, " (writing)", sizeof(buf));
  if (PLR_FLAGGED(i, PLR_MAILING))
    strlcat(buf, " (mailing)", sizeof(buf));
  if (PLR_FLAGGED(i, PLR_EDITING))
    strlcat(buf, " (editing)", sizeof(buf));
  if (PLR_FLAGGED(i, PLR_PROJECT))
    strlcat(buf, " (projecting)", sizeof(buf));
  if (IS_NPC(i) && MOB_FLAGGED(i, MOB_FLAMEAURA))
    strlcat(buf, ", surrounded by flames,", sizeof(buf));
  
  if (GET_QUI(i) <= 0)
    strlcat(buf, " is here, seemingly paralyzed.", sizeof(buf));
  else if (PLR_FLAGGED(i, PLR_MATRIX))
    strlcat(buf, " is here, jacked into a cyberdeck.", sizeof(buf));
  else if (PLR_FLAGGED(i, PLR_REMOTE))
    strlcat(buf, " is here, jacked into a remote control deck.", sizeof(buf));
  else if (AFF_FLAGGED(i, AFF_DESIGN) || AFF_FLAGGED(i, AFF_PROGRAM))
    strlcat(buf, " is here, typing away at a computer.", sizeof(buf));
  else if (AFF_FLAGGED(i, AFF_PART_DESIGN) || AFF_FLAGGED(i, AFF_PART_BUILD))
    strlcat(buf, " is here, working on a cyberdeck.", sizeof(buf));
  else if (AFF_FLAGGED(i, AFF_SPELLDESIGN))
    strlcat(buf, " is here, working on a spell design.", sizeof(buf));
  else if (AFF_FLAGGED(i, AFF_CONJURE))
    strlcat(buf, " is here, performing a conjuring ritual.", sizeof(buf));
  else if (AFF_FLAGGED(i, AFF_LODGE))
    strlcat(buf, " is here, building a shamanic lodge.", sizeof(buf));
  else if (AFF_FLAGGED(i, AFF_CIRCLE))
    strlcat(buf, " is here, drawing a hermetic circle.", sizeof(buf));
  else if (AFF_FLAGGED(i, AFF_PACKING))
    strlcat(buf, " is here, working on a workshop.", sizeof(buf));
  else if (AFF_FLAGGED(i, AFF_BONDING))
    strlcat(buf, " is here, performing a bonding ritual.", sizeof(buf));
  else if (AFF_FLAGGED(i, AFF_PRONE))
    strlcat(buf, " is laying prone here.", sizeof(buf));
  else if (AFF_FLAGGED(i, AFF_PILOT))
  {
    if (AFF_FLAGGED(i, AFF_RIG))
      strlcat(buf, " is plugged into the dashboard.", sizeof(buf));
    else
      strlcat(buf, " is sitting in the drivers seat.", sizeof(buf));
  } else if ((obj = get_mount_manned_by_ch(i))) {
      snprintf(ENDOF(buf), sizeof(buf) - strlen(buf), " is manning %s.", GET_OBJ_NAME(obj));
  } else if (GET_POS(i) != POS_FIGHTING)
  {
    if (GET_DEFPOS(i))
      snprintf(buf2, sizeof(buf2), " %s^n", GET_DEFPOS(i));
    else
      snprintf(buf2, sizeof(buf2), " %s^n.", positions[(int) GET_POS(i)]);
    strlcat(buf, buf2, sizeof(buf));
  } else
  {
    if (FIGHTING(i)) {
      if (AFF_FLAGGED(ch, AFF_BANISH))
        strlcat(buf, " is here, attempting to banish ", sizeof(buf));
      else
        strlcat(buf, " is here, fighting ", sizeof(buf));
      if (FIGHTING(i) == ch)
        strlcat(buf, "YOU!", sizeof(buf));
      else {
        if (i->in_room == FIGHTING(i)->in_room)
          strlcat(buf, PERS(FIGHTING(i), ch), sizeof(buf));
        else
          strlcat(buf, "someone in the distance", sizeof(buf));
        strlcat(buf, "!", sizeof(buf));
      }
    } else if (FIGHTING_VEH(i)) {
      strlcat(buf, " is here, fighting ", sizeof(buf));
      if ((ch->in_veh && ch->in_veh == FIGHTING_VEH(i)) || (ch->char_specials.rigging && ch->char_specials.rigging == FIGHTING_VEH(i)))
        strlcat(buf, "YOU!", sizeof(buf));
      else {
        if (i->in_room == FIGHTING_VEH(i)->in_room)
          strlcat(buf, GET_VEH_NAME(FIGHTING_VEH(i)), sizeof(buf));
        else
          strlcat(buf, "someone in the distance", sizeof(buf));
        strlcat(buf, "!", sizeof(buf));
      }
    } else                      /* NIL fighting pointer */
      strlcat(buf, " is here struggling with thin air.", sizeof(buf));
  }
  
  strlcat(buf, "\r\n", sizeof(buf));
  
  send_to_char(buf, ch);
}

void list_char_to_char(struct char_data * list, struct char_data * ch)
{
  struct char_data *i;
  struct veh_data *veh;
  
  if (!ch || !list)
    return;
  
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
    // TODO: Does this cause double printing if you're inside a nested vehicle and looking out at someone in the containing veh?
    if (!CAN_SEE(ch, i) || !(ch != i || ch->char_specials.rigging) || (ch->in_veh && i->in_veh == ch->in_veh)) {
      continue;
    }
    
    if ((ch->in_veh || (ch->char_specials.rigging))) {
      RIG_VEH(ch, veh);
      
      bool failed = FALSE;
      if (veh->cspeed > SPEED_IDLE) {
        if (get_speed(veh) >= 200) {
          if (success_test(GET_INT(ch) + GET_POWER(ch, ADEPT_IMPROVED_PERCEPT), 7) <= 0) {
            failed = TRUE;
          }
        }
        else if (get_speed(veh) >= 120) {
          if (success_test(GET_INT(ch) + GET_POWER(ch, ADEPT_IMPROVED_PERCEPT), 6) <= 0) {
            failed = TRUE;
          }
        }
        else if (get_speed(veh) >= 60) {
          if (success_test(GET_INT(ch) + GET_POWER(ch, ADEPT_IMPROVED_PERCEPT), 5) <= 0) {
            failed = TRUE;
          }
        }
        else {
          if (success_test(GET_INT(ch) + GET_POWER(ch, ADEPT_IMPROVED_PERCEPT), 4) <= 0) {
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
  buf2[0] = '\0'; // so strlcats will start at the beginning
  
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
        snprintf(buf2, sizeof(buf2), "%-5s - [%5ld] %s%s\r\n", dirs[door],
                EXIT(ch, door)->to_room->number,
                EXIT(ch, door)->to_room->name,
                (IS_SET(EXIT(ch, door)->exit_info, EX_CLOSED) ? " (closed)" : ""));
        if (autom)
          strlcat(buf, "^c", sizeof(buf));
        strlcat(buf, CAP(buf2), sizeof(buf));
      } else if (!IS_SET(EXIT(ch, door)->exit_info, EX_HIDDEN)) {
        snprintf(buf2, sizeof(buf2), "%-5s - ", dirs[door]);
        if (!IS_ASTRAL(ch) &&
            !LIGHT_OK(ch))
          strlcat(buf2, "Too dark to tell.\r\n", sizeof(buf2));
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
                strlcat(buf2, "A pair of closed elevator doors", sizeof(buf2));
            }
            else
              snprintf(ENDOF(buf2), sizeof(buf2) - strlen(buf2), "A closed %s", *(fname(EXIT(ch, door)->keyword)) ? fname(EXIT(ch, door)->keyword) : "door");
          } else
            strlcat(buf2, EXIT(ch, door)->to_room->name, sizeof(buf2));
          strlcat(buf2, "\r\n", sizeof(buf2));
        }
        if (autom)
          strlcat(buf, "^c", sizeof(buf));
        strlcat(buf, CAP(buf2), sizeof(buf));
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
            snprintf(ENDOF(buf), sizeof(buf) - strlen(buf), "(%s) ", exitdirs[door]);
          else if (!IS_SET(EXIT(ch, door)->exit_info, EX_CLOSED | EX_HIDDEN))
            snprintf(ENDOF(buf), sizeof(buf) - strlen(buf), "%s ", exitdirs[door]);
        } else {
          if (!IS_SET(EXIT(ch, door)->exit_info, EX_HIDDEN) || GET_LEVEL(ch) > LVL_MORTAL) {
            if (IS_SET(EXIT(ch, door)->exit_info, EX_LOCKED))
              snprintf(ENDOF(buf), sizeof(buf) - strlen(buf), "%s(L) ", exitdirs[door]);
            else if (IS_SET(EXIT(ch, door)->exit_info, EX_CLOSED))
              snprintf(ENDOF(buf), sizeof(buf) - strlen(buf), "%s(C) ", exitdirs[door]);
            else
              snprintf(ENDOF(buf), sizeof(buf) - strlen(buf), "%s ", exitdirs[door]);
          }
        }
      }
    send_to_char(ch, "^c[ Exits: %s]^n\r\n", *buf ? buf : "None! ");
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
          snprintf(buf, sizeof(buf), "%s the blood staining this area washes away in the rain.", world[i].blood > 3 ? "Some of" : (world[i].blood > 1 ? "Most of" : "The last of"));
          send_to_room(buf, &world[i]);
        } else if (weather_info.sky == SKY_LIGHTNING) {
          world[i].blood -= 2;
          snprintf(buf, sizeof(buf), "%s the blood staining this area washes away in the pounding rain.", world[i].blood > 4 ? "Some of" : (world[i].blood > 1 ? "Most of" : "The last of"));
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
      snprintf(buf, sizeof(buf), "SYSERR: Character %s is not in a room or vehicle.", GET_CHAR_NAME(ch));
      mudlog(buf, ch, LOG_SYSLOG, TRUE);
      send_to_char("ALERT AN IMM!!\r\n", ch);
      return;
    }
    send_to_char(ch, "^CInside %s^n\r\n", GET_VEH_NAME(ch->in_veh), ch);
    send_to_char(ch->vfront ? ch->in_veh->inside_description : ch->in_veh->rear_description, ch);
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
      snprintf(buf, sizeof(buf), "SYSERR: Vehicle %s is not in a room or vehicle.", GET_VEH_NAME(veh));
      mudlog(buf, ch, LOG_SYSLOG, TRUE);
      send_to_char("ALERT AN IMM!!\r\n", ch);
      return;
    }
    if (veh->in_veh) {
      int ov = ch->vfront;
      ch->vfront = FALSE;
      send_to_char(ch, "^CInside %s^n\r\n", GET_VEH_NAME(veh->in_veh), ch);
      send_to_char(veh->in_veh->rear_description, ch);
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
          send_to_char(veh->in_room->night_desc, ch);
        else
          send_to_char(veh->in_room->description, ch);
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
  
  if ((ch->in_veh && !ch->in_room) || PLR_FLAGGED(ch, PLR_REMOTE)) {
    look_in_veh(ch);
    return;
  }
  
  if (!ch->in_room) {
    send_to_char("Your mind is blasted by the eldritch horrors of the unknown void you're drifting in.\r\n", ch);
    snprintf(buf, sizeof(buf), "SYSERR: %s tried to look at their room but is nowhere!", GET_CHAR_NAME(ch));
    mudlog(buf, ch, LOG_SYSLOG, TRUE);
    return;
  }
  
  if ((PRF_FLAGGED(ch, PRF_ROOMFLAGS) && GET_REAL_LEVEL(ch) >= LVL_BUILDER)) {
    ROOM_FLAGS(ch->in_room).PrintBits(buf, MAX_STRING_LENGTH, room_bits, ROOM_MAX);
    send_to_char(ch, "^C[%5ld] %s [ %s ]^n\r\n", GET_ROOM_VNUM(ch->in_room), GET_ROOM_NAME(ch->in_room), buf);
  } else {
    send_to_char(ch, "^C%s^n%s%s%s%s%s%s\r\n", GET_ROOM_NAME(ch->in_room),
                 ROOM_FLAGGED(ch->in_room, ROOM_GARAGE) ? " (Garage)" : "",
                 ROOM_FLAGGED(ch->in_room, ROOM_STORAGE) && !ROOM_FLAGGED(ch->in_room, ROOM_CORPSE_SAVE_HACK) ? " (Storage)" : "",
                 ROOM_FLAGGED(ch->in_room, ROOM_HOUSE) ? " (Apartment)" : "",
                 ROOM_FLAGGED(ch->in_room, ROOM_ARENA) ? " ^y(Arena)^n" : "",
                 ch->in_room->matrix && real_host(ch->in_room->matrix) >= 1 ? " (Jackpoint)" : "",
                 ROOM_FLAGGED(ch->in_room, ROOM_ENCOURAGE_CONGREGATION) ? " ^W(Socialization Bonus)^n" : "");
  }
  
  // TODO: Why is this code here? If you're in a vehicle, you do look_in_veh() above right?
  if (!(ch->in_veh && get_speed(ch->in_veh) > 200)) {
    if (ignore_brief || !PRF_FLAGGED(ch, PRF_BRIEF)) {
      if (ch->in_room->night_desc && weather_info.sunlight == SUN_DARK)
        send_to_char(ch->in_room->night_desc, ch);
      else
        send_to_char(ch->in_room->description, ch);
    }
  }
  
  /* autoexits */
  if (PRF_FLAGGED(ch, PRF_AUTOEXIT))
    do_auto_exits(ch);
  
  if (ch->in_room->blood > 0)
    send_to_char(blood_messages[(int) ch->in_room->blood], ch);
    
  if (ch->in_room->debris > 0) {
    strncpy(buf, "^y", sizeof(buf));
    
    if (ch->in_room->debris < 5) {
      strlcat(buf, "A few spent casings and empty mags are scattered here.", sizeof(buf));
    }
    else if (ch->in_room->debris < 10) {
      strlcat(buf, "Spent casings and empty mags are scattered here.", sizeof(buf));
    }
    else if (ch->in_room->debris < 20) {
      strlcat(buf, "Bullet holes, spent casings, and empty mags litter the area.", sizeof(buf));
    }
    else if (ch->in_room->debris < 30) {
      strlcat(buf, "This place has been shot up, and weapons debris is everywhere.", sizeof(buf));
    }
    else if (ch->in_room->debris < 40) {
      strlcat(buf, "The acrid scent of propellant hangs in the air amidst the weapons debris.", sizeof(buf));
    }
    else if (ch->in_room->debris < 45) {
      strlcat(buf, "Veritable piles of spent casings and empty mags fill the area.", sizeof(buf));
    }
    else {
      strncpy(buf, "^YIt looks like World War III was fought here!", sizeof(buf));
    }
    strlcat(buf, "^n\r\n", sizeof(buf));
    send_to_char(buf, ch);
  }
  
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
          snprintf(buf, sizeof(buf), "SYSERR: '%s' (%ld) has powersite background count %d, which is greater than displayable max of 5.",
                  ch->in_room->name, ch->in_room->number, GET_BACKGROUND_COUNT(ch->in_room));
          mudlog(buf, ch, LOG_SYSLOG, TRUE);
          break;
      }
    } else if (GET_BACKGROUND_COUNT(ch->in_room) < 6) {
      strncpy(buf, "^cA", sizeof(buf));
      switch (GET_BACKGROUND_COUNT(ch->in_room)) {
        case 1:
          strlcat(buf, " distracting", sizeof(buf));
          break;
        case 2:
          strlcat(buf, " light", sizeof(buf));
          break;
        case 3:
          strlcat(buf, " heavy", sizeof(buf));
          break;
        case 4:
          strlcat(buf, "n almost overwhelming", sizeof(buf));
          break;
        case 5:
          strlcat(buf, "n overwhelming", sizeof(buf));
          break;
      }
      snprintf(ENDOF(buf), sizeof(buf) - strlen(buf), " aura of %s pervades the area.^n\r\n", background_types[GET_BACKGROUND_AURA(ch->in_room)]);
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
    // Iterate through elevators to find one that contains this shaft.
    for (int index = 0; index < num_elevators; index++) {
      int car_rating = world[real_room(elevator[index].room)].rating;
      if (elevator[index].floor[car_rating].shaft_vnum == ch->in_room->number) {
        // Check for the car being at this floor.
        if (IS_ASTRAL(ch)) {
          send_to_char("^yThe massive bulk of an elevator car fills the hoistway.^n\r\n", ch);
        } else {
          send_to_char("^RThe massive bulk of an elevator car fills the hoistway, squeezing you aside.^n\r\n", ch);
        }
        
        break;
      }
    }
  }
  
  SPECIAL(car_dealer);
  if (ch->in_room->func) {
    if (ch->in_room->func == car_dealer) {
      send_to_char("^y...There are vehicles for sale here.^n\r\n", ch);
    }
    
    // TODO: Add any other relevant funcs with room displays here.
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
      if (success_test(GET_INT(ch) + GET_POWER(ch, ADEPT_IMPROVED_PERCEPT), EXIT(ch, dir)->hidden) <= 0) {
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
    if (str_str("pockets", arg)) {
      send_to_char("Please see ^WHELP POCKETS^n for info on how to use your ammo pockets.\r\n", ch);
    } else {
      send_to_char(ch, "There doesn't seem to be %s %s here.\r\n", AN(arg), arg);
    }
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
             (GET_OBJ_TYPE(obj) != ITEM_KEYRING) &&
             (GET_OBJ_TYPE(obj) != ITEM_GUN_AMMO)
           )
    send_to_char("There's nothing inside that!\r\n", ch);
  else
  {
    if (GET_OBJ_TYPE(obj) == ITEM_GUN_AMMO) {
      send_to_char(ch, "It contains %d %s.\r\n", 
                   GET_AMMOBOX_QUANTITY(obj),
                   get_ammo_representation(GET_AMMOBOX_WEAPON(obj), GET_AMMOBOX_TYPE(obj), GET_AMMOBOX_QUANTITY(obj)));
      return;
    } else if (GET_OBJ_TYPE(obj) == ITEM_WORN) {
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
        snprintf(buf, sizeof(buf), "It's %sfull of a %s liquid.\r\n", fullness[amt],
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
                      FIND_CHAR_ROOM | FIND_OBJ_VEH_ROOM | FIND_CHAR_VEH_ROOM, ch, &found_char, &found_obj);
  
  // Look at self.
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
  
  // Look at vehicles, either in the back of a vehicle (look at inside ones) or outside of a vehicle.
  if (!ch->in_veh || (ch->in_veh && !ch->vfront)) {
    found_veh = get_veh_list(arg, ch->in_veh ? ch->in_veh->carriedvehs : ch->in_room->vehicles, ch);
  }
  
  if (ch->in_veh) {
    // Look at outside vehicles from within a vehicle.
    found_veh = get_veh_list(arg, (get_ch_in_room(ch))->vehicles, ch);
  }
  
  if (found_veh) {
    send_to_char(GET_VEH_DESC(found_veh), ch);
    if (PLR_FLAGGED(ch, PLR_REMOTE))
      ch->in_room = was_in;
    return;
  }
  
  /* Is the target a character? */
  if (found_char != NULL)
  {
    look_at_char(found_char, ch);
    /*
    if (ch != found_char && !ch->char_specials.rigging) {
      if (CAN_SEE(found_char, ch))
        act("$n looks at you.", TRUE, ch, 0, found_char, TO_VICT);
      act("$n looks at $N.", TRUE, ch, 0, found_char, TO_NOTVICT);
    }
    */
    ch->in_room = was_in;
    return;
  } else if (ch->in_veh)
  {
    found_char = get_char_veh(ch, arg, ch->in_veh);
    if (found_char) {
      look_at_char(found_char, ch);
      if (ch != found_char) {
        if (CAN_SEE(found_char, ch)) {
          send_to_char(found_char, "%s looks at you.\r\n", GET_NAME(ch));
        }
        snprintf(buf, sizeof(buf), "%s looks at %s.\r\n", GET_NAME(ch), GET_NAME(found_char));
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
  if (ch->in_room) {
    FOR_ITEMS_AROUND_CH(ch, obj) {
      if (CAN_SEE_OBJ(ch, obj)) {
        if ((desc = find_exdesc(arg, obj->ex_description)) != NULL) {
          send_to_char(desc, ch);
          found = 1;
        }
      }
    }
  }
  if (bits)
  {                   /* If an object was found back in
                       * generic_find */
    if (!found)
      show_obj_to_char(found_obj, ch, 5);       /* Show no-description */
    else
      show_obj_to_char(found_obj, ch, 6);       /* Find hum, glow etc */
  } else if (!found) {
    if (str_str("pockets", arg)) {
      send_to_char("Please see ^WHELP POCKETS^n for info on how to use your ammo pockets.\r\n", ch);
    } else {
      send_to_char(ch, "You don't see anything named '%s' here.\r\n", arg);
    }
  }
    
  ch->in_room = was_in;
  
}

ACMD_CONST(do_look) {
  char not_const[MAX_STRING_LENGTH];
  strlcpy(not_const, argument, sizeof(not_const));
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
      do_examine(ch, arg2, 0, SCMD_EXAMINE);
    else
      do_examine(ch, arg, 0, SCMD_EXAMINE);
    /* else if (is_abbrev(arg, "at"))
      look_at_target(ch, arg2);
    else
      look_at_target(ch, arg);
      */
  }
}

void look_at_veh(struct char_data *ch, struct veh_data *veh, int success)
{
  strlcpy(buf, GET_VEH_NAME(veh), sizeof(buf));
  int cond = 10 - veh->damage;
  if (cond >= 10)
    strlcat(buf, " is in perfect condition.\r\n", sizeof(buf));
  else if (cond >= 9)
    strlcat(buf, " has some light scratches.\r\n", sizeof(buf));
  else if (cond >= 5)
    strlcat(buf, " is dented and scratched.\r\n", sizeof(buf));
  else if (cond >= 3)
    strlcat(buf, " has seen better days.\r\n", sizeof(buf));
  else if (cond >= 1)
    strlcat(buf, " is barely holding together.\r\n", sizeof(buf));
  else
    strlcat(buf, " is wrecked.\r\n", sizeof(buf));
  send_to_char(buf, ch);
  disp_mod(veh, ch, success);
}

void do_probe_veh(struct char_data *ch, struct veh_data * k)
{
  snprintf(buf, sizeof(buf), "Name: '^y%s^n', Aliases: %s\r\n",
          k->short_description, k->name);
  snprintf(ENDOF(buf), sizeof(buf) - strlen(buf), "It is a ^c%s^n with handling ^c%d^n, a top speed of ^c%d^n, and raw acceleration of ^c%d^n.\r\n",
          veh_type[k->type], k->handling, k->speed, k->accel);
  snprintf(ENDOF(buf), sizeof(buf) - strlen(buf), "It has a body rating of ^c%d^n and rating-^c%d^n vehicular armor. It seats ^c%d^n up front and ^c%d^n in the back.\r\n",
          k->body, k->armor, k->seating[1], k->seating[0]);
  snprintf(ENDOF(buf), sizeof(buf) - strlen(buf), "Its signature rating is ^c%d^n, and its NERP pilot rating is ^c%d^n.\r\n",
          k->sig, k->pilot);
  snprintf(ENDOF(buf), sizeof(buf) - strlen(buf), "It has ^c%d^n slots in its autonav and carrying capacity of ^c%d^n (%d in use).\r\n",
          k->autonav, (int)k->load, (int)k->usedload);
          snprintf(ENDOF(buf), sizeof(buf) - strlen(buf), "Its engine is adapted for ^c%s^n. If loaded into another vehicle, it takes up ^c%d^n load.\r\n",
                  engine_type[k->engine], calculate_vehicle_entry_load(k));
  send_to_char(buf, ch);
}

void do_probe_object(struct char_data * ch, struct obj_data * j) {
  int i, found, mount_location, bal, imp;
  bool has_pockets = FALSE, added_extra_carriage_return = FALSE, has_smartlink = FALSE;
  struct obj_data *access = NULL;
  
  if (j->restring) {
    snprintf(buf, sizeof(buf), "^MOOC^n statistics for '^y%s^n' (restrung from %s):\r\n", GET_OBJ_NAME(j), j->text.name);
  } else {
    snprintf(buf, sizeof(buf), "^MOOC^n statistics for '^y%s^n':\r\n", (GET_OBJ_NAME(j) ? GET_OBJ_NAME(j) : "<None>"));
  }
  
  sprinttype(GET_OBJ_TYPE(j), item_types, buf1, sizeof(buf1));
  snprintf(ENDOF(buf), sizeof(buf) - strlen(buf), "It is %s ^c%s^n that weighs ^c%.2f^n kilos. It is made of ^c%s^n with a durability of ^c%d^n.\r\n",
          AN(buf1), buf1, GET_OBJ_WEIGHT(j), material_names[(int)GET_OBJ_MATERIAL(j)], GET_OBJ_BARRIER(j));
  
  bool was_take = j->obj_flags.wear_flags.IsSet(ITEM_WEAR_TAKE);
  j->obj_flags.wear_flags.RemoveBit(ITEM_WEAR_TAKE);
  if (strcmp(j->obj_flags.wear_flags.ToString(), "0") != 0) {
    j->obj_flags.wear_flags.PrintBits(buf2, MAX_STRING_LENGTH, wear_bits, NUM_WEARS);
    snprintf(ENDOF(buf), sizeof(buf) - strlen(buf), "It can be worn or equipped at the following wear location(s):\r\n  ^c%s^n\r\n", buf2);
  } else {
    strlcat(buf, "This item cannot be worn or equipped.\r\n", sizeof(buf));
  }
  if (was_take)
    j->obj_flags.wear_flags.SetBit(ITEM_WEAR_TAKE);
  else
    strlcat(buf, "^yIt cannot be picked up once dropped.^n\r\n", sizeof(buf));
  
  switch (GET_OBJ_TYPE(j))
  {
    case ITEM_LIGHT:
      if (GET_OBJ_VAL(j, 2) == -1) {
        strlcat(buf, "It is an ^cinfinite^n light source.", sizeof(buf));
      } else {
        snprintf(ENDOF(buf), sizeof(buf) - strlen(buf), "It has ^c%d^n hours of light left.", GET_OBJ_VAL(j, 2));
      }
      break;
    case ITEM_FIREWEAPON:
      strlcat(buf, "As a fireweapon, it is not currently implemented.\r\n", sizeof(buf));
      /*
      snprintf(ENDOF(buf), sizeof(buf) - strlen(buf), "It is a ^c%s^n that requires ^c%d^n strength to use in combat.\r\n",
              GET_OBJ_VAL(j, 5) == 0 ? "Bow" : "Crossbow", GET_OBJ_VAL(j, 6));
      if (GET_OBJ_VAL(j, 2)) {
        snprintf(ENDOF(buf), sizeof(buf) - strlen(buf), "It has a damage code of ^c(STR+%d)%s%s^n", GET_OBJ_VAL(j, 2), wound_arr[GET_OBJ_VAL(j, 1)],
                !IS_DAMTYPE_PHYSICAL(get_weapon_damage_type(j)) ? " (stun)" : "");
      } else {
        snprintf(ENDOF(buf), sizeof(buf) - strlen(buf), "It has a damage code of ^c(STR)%s%s^n", wound_arr[GET_OBJ_VAL(j, 1)],
                !IS_DAMTYPE_PHYSICAL(get_weapon_damage_type(j)) ? " (stun)" : "");
      }
      snprintf(ENDOF(buf), sizeof(buf) - strlen(buf), " and requires the ^c%s^n skill to use.", skills[GET_OBJ_VAL(j, 4)].name);
      */
      break;
    case ITEM_WEAPON:
      // Ranged weapons first.
      if (IS_GUN(GET_OBJ_VAL((j), 3))) {
        int burst_count = 0;
        if (GET_OBJ_VAL(j, 5) > 0) {
          snprintf(ENDOF(buf), sizeof(buf) - strlen(buf), "It is a ^c%d-round %s^n that uses the ^c%s^n skill to fire.",
                  GET_OBJ_VAL(j, 5), weapon_type[GET_OBJ_VAL(j, 3)], skills[GET_OBJ_VAL(j, 4)].name);
        } else {
          snprintf(ENDOF(buf), sizeof(buf) - strlen(buf), "It is %s ^c%s^n that uses the ^c%s^n skill to fire.",
                  AN(weapon_type[GET_OBJ_VAL(j, 3)]), weapon_type[GET_OBJ_VAL(j, 3)], skills[GET_OBJ_VAL(j, 4)].name);
        }
        // Damage code.
        snprintf(ENDOF(buf), sizeof(buf) - strlen(buf), " Its base damage code is ^c%d%s%s^n",
                 GET_OBJ_VAL(j, 0), wound_arr[GET_OBJ_VAL(j, 1)], !IS_DAMTYPE_PHYSICAL(get_weapon_damage_type(j)) ? " (stun)" : "");
        
        // Burst fire?
        if (GET_WEAPON_FIREMODE(j) == MODE_BF || GET_WEAPON_FIREMODE(j) == MODE_FA) {
          burst_count = GET_WEAPON_FIREMODE(j) == MODE_BF ? 3 : GET_WEAPON_FULL_AUTO_COUNT(j);
          
          snprintf(ENDOF(buf), sizeof(buf) - strlen(buf), ", raised to ^c%d%s%s^n by its current fire mode.",
                   GET_WEAPON_POWER(j) + burst_count,
                   wound_arr[MIN(DEADLY, GET_WEAPON_DAMAGE_CODE(j) + (int)(burst_count / 3))],
                   !IS_DAMTYPE_PHYSICAL(get_weapon_damage_type(j)) ? " (stun)" : "");
        } else {
          strlcat(buf, ".", sizeof(buf));
        }
        
        if (j->contains 
            && GET_OBJ_TYPE(j->contains) == ITEM_GUN_MAGAZINE
            && GET_MAGAZINE_AMMO_COUNT(j->contains) > 0
            && GET_MAGAZINE_AMMO_TYPE(j->contains) != AMMO_NORMAL) {
          snprintf(ENDOF(buf), sizeof(buf) - strlen(buf), "\r\nIt is loaded with %s, which ", 
                   ammo_type[GET_MAGAZINE_AMMO_TYPE(j->contains)].name);
          switch (GET_MAGAZINE_AMMO_TYPE(j->contains)) {
            case AMMO_APDS:
              strlcat(buf, "pierces through enemy ballistic armor, halving its value.", sizeof(buf));
              break;
            case AMMO_EX:
              strlcat(buf, "increases power by two.", sizeof(buf));
              break;
            case AMMO_EXPLOSIVE:
              strlcat(buf, "increases power by one.", sizeof(buf));
              break;
            case AMMO_FLECHETTE:
              strlcat(buf, "deals more damage to fully unarmored targets.", sizeof(buf));
              break;
            case AMMO_GEL:
              strlcat(buf, "deals mental instead of physical damage, but treats the enemy's ballistic armor as two points higher.", sizeof(buf));
              break;
          }
        }
        
        if (GET_WEAPON_INTEGRAL_RECOIL_COMP(j)) {
          snprintf(ENDOF(buf), sizeof(buf) - strlen(buf), "\r\nIt has ^c%d^n point%s of integral recoil compensation.",
                  GET_WEAPON_INTEGRAL_RECOIL_COMP(j),
                  GET_WEAPON_INTEGRAL_RECOIL_COMP(j) > 1 ? "s" : "");
        }
        
        // Info about attachments, if any.
        int standing_recoil_comp = GET_WEAPON_INTEGRAL_RECOIL_COMP(j);
        int prone_recoil_comp = 0;
        int real_obj;
        for (int i = ACCESS_LOCATION_TOP; i <= ACCESS_LOCATION_UNDER; i++) {
          if (GET_OBJ_VAL(j, i) > 0
              && (real_obj = real_object(GET_OBJ_VAL(j, i))) > 0
              && (access = &obj_proto[real_obj])) {
            // mount_location: used for gun_accessory_locations[] lookup.
            mount_location = i - ACCESS_LOCATION_TOP;
            
            // format flair
            if (!added_extra_carriage_return) {
              strlcat(buf, "\r\n", sizeof(buf));
              added_extra_carriage_return = TRUE;
            }
            
            // parse and add the string for the accessory's special bonuses
            switch (GET_OBJ_VAL(access, 1)) {
              case ACCESS_SMARTLINK:
                has_smartlink = TRUE;
                snprintf(ENDOF(buf), sizeof(buf) - strlen(buf), "\r\nA Smartlink%s attached to the %s provides ^c%d^n to target numbers (lower is better).",
                        GET_OBJ_VAL(access, 2) < 2 ? "" : "-II", gun_accessory_locations[mount_location],
                        (GET_OBJ_VAL(j, 1) == 1 || GET_OBJ_VAL(access, 2) < 2) ? -2 : -4);
                break;
              case ACCESS_SCOPE:
                if (GET_OBJ_AFFECT(access).IsSet(AFF_LASER_SIGHT)) {
                  snprintf(ENDOF(buf), sizeof(buf) - strlen(buf), "\r\nA laser sight attached to the %s provides ^c-1^n to target numbers (lower is better).",
                          gun_accessory_locations[mount_location]);
                } else {
                  snprintf(ENDOF(buf), sizeof(buf) - strlen(buf), "\r\nA scope has been attached to the %s.",
                          gun_accessory_locations[mount_location]);
                }
                break;
              case ACCESS_GASVENT:
                snprintf(ENDOF(buf), sizeof(buf) - strlen(buf), "\r\nA gas vent installed in the %s provides ^c%d^n round%s worth of recoil compensation.",
                        gun_accessory_locations[mount_location], -GET_OBJ_VAL(access, 2), -GET_OBJ_VAL(access, 2) > 1 ? "s'" : "'s");
                standing_recoil_comp -= GET_OBJ_VAL(access, 2);
                break;
              case ACCESS_SHOCKPAD:
                snprintf(ENDOF(buf), sizeof(buf) - strlen(buf), "\r\nA shock pad installed on the %s provides ^c1^n round's worth of recoil compensation.",
                        gun_accessory_locations[mount_location]);
                standing_recoil_comp++;
                break;
              case ACCESS_SILENCER:
                snprintf(ENDOF(buf), sizeof(buf) - strlen(buf), "\r\nThe silencer installed on the %s will muffle this weapon's report.",
                        gun_accessory_locations[mount_location]);
                break;
              case ACCESS_SOUNDSUPP:
                snprintf(ENDOF(buf), sizeof(buf) - strlen(buf), "\r\nThe suppressor installed on the %s will muffle this weapon's report.",
                        gun_accessory_locations[mount_location]);
                break;
              case ACCESS_SMARTGOGGLE:
                strlcat(buf, "\r\n^RYou should never see this-- alert an imm.^n", sizeof(buf));
                break;
              case ACCESS_BIPOD:
                snprintf(ENDOF(buf), sizeof(buf) - strlen(buf), "\r\nA bipod installed on the %s provides ^c%d^n round%s worth of recoil compensation when fired from prone.",
                        gun_accessory_locations[mount_location], RECOIL_COMP_VALUE_BIPOD, RECOIL_COMP_VALUE_BIPOD > 1 ? "s'" : "'s");
                prone_recoil_comp += RECOIL_COMP_VALUE_BIPOD;
                break;
              case ACCESS_TRIPOD:
                snprintf(ENDOF(buf), sizeof(buf) - strlen(buf), "\r\nA tripod installed on the %s provides ^c%d^n round%s worth of recoil compensation when fired from prone.",
                        gun_accessory_locations[mount_location], RECOIL_COMP_VALUE_TRIPOD, RECOIL_COMP_VALUE_TRIPOD > 1 ? "s'" : "'s");
                prone_recoil_comp += RECOIL_COMP_VALUE_TRIPOD;
                break;
              case ACCESS_BAYONET:
                if (mount_location != ACCESS_ACCESSORY_LOCATION_UNDER) {
                  strlcat(buf, "\r\n^RYour bayonet has been mounted in the wrong location and is nonfunctional. Alert an imm.", sizeof(buf));
                } else {
                  snprintf(ENDOF(buf), sizeof(buf) - strlen(buf), "\r\nA bayonet attached to the %s allows you to use the Pole Arms skill when defending from melee attacks.",
                          gun_accessory_locations[mount_location]);
                }
                break;
              default:
                snprintf(buf1, sizeof(buf1), "SYSERR: Unknown accessory type %d passed to do_probe_object()", GET_OBJ_VAL(access, 1));
                log(buf1);
                break;
            }
            // Tack on affect and extra flags to the attachment.
            if (strcmp(GET_OBJ_AFFECT(access).ToString(), "0") != 0) {
              GET_OBJ_AFFECT(access).PrintBits(buf2, MAX_STRING_LENGTH, affected_bits, AFF_MAX);
              snprintf(ENDOF(buf), sizeof(buf) - strlen(buf), "\r\n ^- It provides the following flags: ^c%s^n", buf2);
            }
            
            if (strcmp(GET_OBJ_EXTRA(access).ToString(), "0") != 0) {
              GET_OBJ_EXTRA(access).PrintBits(buf2, MAX_STRING_LENGTH, pc_readable_extra_bits, ITEM_EXTRA_MAX);
              snprintf(ENDOF(buf), sizeof(buf) - strlen(buf), "\r\n ^- It provides the following extra features: ^c%s^n", buf2);
            }
          }
        }
        
        if (burst_count > 0 && burst_count - standing_recoil_comp > 0) {
          strlcat(buf, "\r\n\r\n^yIt doesn't have enough recoil compensation", sizeof(buf));
          if (burst_count - standing_recoil_comp - prone_recoil_comp <= 0) {
            strlcat(buf, " unless fired from prone", sizeof(buf));
          } else if (prone_recoil_comp > 0){
            strlcat(buf, " even when fired from prone", sizeof(buf));
          }
          // Do we require more recoil comp than is currently attached?
          switch (GET_WEAPON_SKILL(j)) {
            case SKILL_SHOTGUNS:
            case SKILL_MACHINE_GUNS:
            case SKILL_ASSAULT_CANNON:
              snprintf(ENDOF(buf), sizeof(buf) - strlen(buf), ".^n\r\nStanding recoil penalty (doubled): ^c%d^n.  Prone recoil penalty (doubled): ^c%d^n.",
                       (burst_count - standing_recoil_comp) * 2, MAX(0, burst_count - standing_recoil_comp - prone_recoil_comp) * 2);
              break;
            default:
              snprintf(ENDOF(buf), sizeof(buf) - strlen(buf), ".^n\r\nStanding recoil penalty: ^c%d^n.  Prone recoil penalty: ^c%d^n.",
                      burst_count - standing_recoil_comp, MAX(0, burst_count - standing_recoil_comp - prone_recoil_comp));
              break;
          }
        }
      }
      // Melee weapons.
      else {
        if (obj_index[GET_OBJ_RNUM(j)].wfunc == monowhip) {
          snprintf(ENDOF(buf), sizeof(buf) - strlen(buf), "It is a ^c%s^n that uses the ^c%s^n skill to attack with. Its damage code is ^c10%s%s^n.",
                  weapon_type[GET_OBJ_VAL(j, 3)],
                  skills[GET_OBJ_VAL(j, 4)].name,
                  wound_arr[GET_OBJ_VAL(j, 1)],
                  !IS_DAMTYPE_PHYSICAL(get_weapon_damage_type(j)) ? " (stun)" : "");
        }
        else if (GET_OBJ_VAL(j, 2) != 0) {
          snprintf(ENDOF(buf), sizeof(buf) - strlen(buf), "It is a ^c%s^n that uses the ^c%s^n skill to attack with. Its damage code is ^c(STR%s%d)%s%s^n.",
                  weapon_type[GET_OBJ_VAL(j, 3)],
                  skills[GET_OBJ_VAL(j, 4)].name,
                  GET_OBJ_VAL(j, 2) < 0 ? "" : "+",
                  GET_OBJ_VAL(j, 2),
                  wound_arr[GET_OBJ_VAL(j, 1)],
                  !IS_DAMTYPE_PHYSICAL(get_weapon_damage_type(j)) ? " (stun)" : "");
        } else {
          snprintf(ENDOF(buf), sizeof(buf) - strlen(buf), "It is a ^c%s^n that uses the ^c%s^n skill to attack with. Its damage code is ^c(STR)%s%s^n.",
                  weapon_type[GET_OBJ_VAL(j, 3)], skills[GET_OBJ_VAL(j, 4)].name, wound_arr[GET_OBJ_VAL(j, 1)],
                  !IS_DAMTYPE_PHYSICAL(get_weapon_damage_type(j)) ? " (stun)" : "");
        }
        if (GET_WEAPON_REACH(j)) {
          snprintf(ENDOF(buf), sizeof(buf) - strlen(buf), "\r\nIt grants ^c%d^n meters of reach when wielded.", GET_WEAPON_REACH(j));
        }
        if (GET_WEAPON_FOCUS_RATING(j) > 0) {
          snprintf(ENDOF(buf), sizeof(buf) - strlen(buf), "\r\nIt is a weapon focus of rating ^c%d^n. When bonded and wielded, its rating is added to your weapon skill.", GET_WEAPON_FOCUS_RATING(j));
        }
      }
      break;
    case ITEM_MISSILE:
      if (GET_OBJ_VAL(j, 0) == 0) {
        strlcat(buf, "It is an ^carrow^n suitable for use in a bow.", sizeof(buf));
      } else {
        strlcat(buf, "It is a ^cbolt^n suitable for use in a crossbow.", sizeof(buf));
      }
      break;
    case ITEM_WORN:
      strncpy(buf1, "It has space for ", sizeof(buf1));
      
      if (GET_WORN_POCKETS_HOLSTERS(j) > 0) {
        snprintf(ENDOF(buf1), sizeof(buf1) - strlen(buf1), "%s^c%d^n holster%s", 
                 (has_pockets?", ":""), GET_WORN_POCKETS_HOLSTERS(j), GET_WORN_POCKETS_HOLSTERS(j) > 1 ? "s":"");
        has_pockets = TRUE;
      }
      /*   Magazines aren't a thing anymore. -- LS
      if (GET_OBJ_VAL(j, 1) > 0) {
        snprintf(ENDOF(buf1), sizeof(buf1) - strlen(buf1), "%s^c%d^n magazine%s", 
                 (has_pockets?", ":""), GET_OBJ_VAL(j, 1), GET_OBJ_VAL(j, 1) > 1 ? "s":"");
        has_pockets = TRUE;
      } */
      if (GET_WORN_POCKETS_MISC(j) > 0) {
        snprintf(ENDOF(buf1), sizeof(buf1) - strlen(buf1), "%s^c%d^n miscellaneous small item%s", 
                 (has_pockets?", ":""), GET_WORN_POCKETS_MISC(j), GET_WORN_POCKETS_MISC(j) > 1 ? "s":"");
        has_pockets = TRUE;
      }
      
      if (has_pockets) {
        snprintf(ENDOF(buf), sizeof(buf) - strlen(buf), "%s.\r\n", buf1);
      } else {
        strlcat(buf, "It has no pockets.\r\n", sizeof(buf));
      }
      bal = GET_WORN_BALLISTIC(j);
      imp = GET_WORN_IMPACT(j);
      if (GET_WORN_MATCHED_SET(j)) {
        snprintf(ENDOF(buf), sizeof(buf) - strlen(buf), "It is part of matched set number ^c%d^n. Wear all the matched items to receive its full value.\r\n", GET_WORN_MATCHED_SET(j));
        snprintf(ENDOF(buf), sizeof(buf) - strlen(buf), "It provides ^c%.2f^n ballistic armor and ^c%.2f^n impact armor. "
                                                        "People have a ^c%d^n target number when trying to see under it.\r\n", (float)bal / 100, (float)imp / 100, GET_WORN_CONCEAL_RATING(j));
      } else {
        snprintf(ENDOF(buf), sizeof(buf) - strlen(buf), "It provides ^c%d^n ballistic armor and ^c%d^n impact armor. "
                                                        "People have a ^c%d^n target number when trying to see under it.\r\n", bal, imp, GET_WORN_CONCEAL_RATING(j));
      }
      break;
    case ITEM_DOCWAGON:
      snprintf(ENDOF(buf), sizeof(buf) - strlen(buf), "It is a ^c%s^n contract that ^c%s bonded%s^n.",
              docwagon_contract_types[GET_OBJ_VAL(j, 0)], GET_OBJ_VAL(j, 1) ? "is" : "has not been",
              GET_OBJ_VAL(j, 1) ? (GET_OBJ_VAL(j, 1) == GET_IDNUM(ch) ? " to you" : " to someone else") : " to anyone yet");
      break;
    case ITEM_CONTAINER:
      snprintf(ENDOF(buf), sizeof(buf) - strlen(buf), "It can hold a maximum of ^c%d^n kilograms.", GET_OBJ_VAL(j, 0));
      break;
    case ITEM_DRINKCON:
    case ITEM_FOUNTAIN:
      sprinttype(GET_OBJ_VAL(j, 2), drinks, buf2, sizeof(buf2));
      snprintf(ENDOF(buf), sizeof(buf) - strlen(buf), "It currently contains ^c%d/%d^n units of ^c%s^n.",
              GET_OBJ_VAL(j, 0), GET_OBJ_VAL(j, 1), buf2);
      break;
    case ITEM_MONEY:
      if (!GET_OBJ_VAL(j, 1))
        snprintf(ENDOF(buf), sizeof(buf) - strlen(buf), "It is a stack of cash worth ^c%d^n nuyen.", GET_OBJ_VAL(j, 0));
      else
        snprintf(ENDOF(buf), sizeof(buf) - strlen(buf), "It is a ^c%s^n-secured ^ccredstick^n loaded with ^c%d^n nuyen.",
                (GET_OBJ_VAL(j, 2) == 1 ? "6-digit PIN" : (GET_OBJ_VAL(j, 2) == 2 ? "thumbprint" : "retina")), GET_OBJ_VAL(j, 0));
      break;
    case ITEM_KEY:
      strlcat(buf, "No OOC information is available about this key.", sizeof(buf));
      break;
    case ITEM_FOOD:
      snprintf(ENDOF(buf), sizeof(buf) - strlen(buf), "It provides ^c%d^n units of nutrition when eaten.", GET_OBJ_VAL(j, 0));
      break;
    case ITEM_QUIVER:
      if (GET_OBJ_VAL(j, 1) >= 0 && GET_OBJ_VAL(j, 1) <= 3) {
        snprintf(ENDOF(buf), sizeof(buf) - strlen(buf), "It can hold up to ^c%d^n ^c%s%s^n.", GET_OBJ_VAL(j, 0), projectile_ammo_types[GET_OBJ_VAL(j, 1)],
                GET_OBJ_VAL(j, 0) > 1 ? "s" : "");
      } else {
        snprintf(ENDOF(buf), sizeof(buf) - strlen(buf), "It can hold up to ^c%d^n ^cundefined projectiles^n.", GET_OBJ_VAL(j, 0));
      }
      break;
    case ITEM_PATCH:
      snprintf(ENDOF(buf), sizeof(buf) - strlen(buf), "It is a ^crating-%d %s^n patch.", GET_OBJ_VAL(j, 1), patch_names[GET_OBJ_VAL(j, 0)]);
      break;
    case ITEM_CYBERDECK:
      snprintf(ENDOF(buf), sizeof(buf) - strlen(buf), "MPCP: ^c%d^n, Hardening: ^c%d^n, Active: ^c%d^n, Storage: ^c%d^n, Load: ^c%d^n.",
              GET_OBJ_VAL(j, 0), GET_OBJ_VAL(j, 1), GET_OBJ_VAL(j, 2),
              GET_OBJ_VAL(j, 3), GET_OBJ_VAL(j, 4));
      break;
    case ITEM_PROGRAM:
      snprintf(ENDOF(buf), sizeof(buf) - strlen(buf), "It is a ^crating-%d %s^n program, ^c%d^n units in size.",
              GET_OBJ_VAL(j, 1), programs[GET_OBJ_VAL(j, 0)].name, GET_OBJ_VAL(j, 2));
      if (GET_OBJ_VAL(j, 0) == SOFT_ATTACK)
        snprintf(ENDOF(buf), sizeof(buf) - strlen(buf), " Its damage code is ^c%s^n.", wound_name[GET_OBJ_VAL(j, 3)]);
      break;
    case ITEM_BIOWARE:
      snprintf(ENDOF(buf), sizeof(buf) - strlen(buf), "It is a ^crating-%d %s%s^n that uses ^c%.2f^n index when installed.",
              GET_OBJ_VAL(j, 1), GET_OBJ_VAL(j, 2) || GET_OBJ_VAL(j, 0) >= BIO_CEREBRALBOOSTER ? "cultured " : "",
              decap_bio_types[GET_OBJ_VAL(j, 0)], ((float) GET_OBJ_VAL(j, 4) / 100));
      break;
    case ITEM_CYBERWARE:
      snprintf(ENDOF(buf), sizeof(buf) - strlen(buf), "It is a ^crating-%d %s-grade %s^n that uses ^c%.2f^n essence when installed.",
              GET_OBJ_VAL(j, 1), decap_cyber_grades[GET_OBJ_VAL(j, 2)], decap_cyber_types[GET_OBJ_VAL(j, 0)],
              ((float) GET_OBJ_VAL(j, 4) / 100));
      break;
    case ITEM_WORKSHOP:
      snprintf(ENDOF(buf), sizeof(buf) - strlen(buf), "It is a ^c%s^n designed for ^c%s^n.",
              GET_OBJ_VAL(j, 1) ? GET_OBJ_VAL(j, 1) == 3 ? "Facility": "Workshop" : "Kit", workshops[GET_OBJ_VAL(j, 0)]);
      break;
    case ITEM_FOCUS:
      snprintf(ENDOF(buf), sizeof(buf) - strlen(buf), "It is a ^c%s^n focus of force ^c%d^n.", foci_type[GET_OBJ_VAL(j, 0)], GET_OBJ_VAL(j, 1));
      break;
    case ITEM_SPELL_FORMULA:
      snprintf(ENDOF(buf), sizeof(buf) - strlen(buf), "It is a ^cforce-%d %s^n designed for ^c%s^n mages.", GET_OBJ_VAL(j, 0),
              spells[GET_OBJ_VAL(j, 1)].name, GET_OBJ_VAL(j, 2) ? "Shamanic" : "Hermetic");
      break;
    case ITEM_PART:
      snprintf(ENDOF(buf), sizeof(buf) - strlen(buf), "It is %s %s^c%s^n designed for MPCP ^c%d^n decks. It will cost %d nuyen in parts and %d nuyen in chips to build.", 
               AN(parts[GET_OBJ_VAL(j, 0)].name),
               !GET_PART_DESIGN_COMPLETION(j) ? "not-yet-designed " : "",
               parts[GET_OBJ_VAL(j, 0)].name, 
               GET_OBJ_VAL(j, 2),
               GET_PART_PART_COST(j),
               GET_PART_CHIP_COST(j)
             );
      break;
    case ITEM_CUSTOM_DECK:
      snprintf(ENDOF(buf), sizeof(buf) - strlen(buf), "You should EXAMINE this deck, or jack in and view its SOFTWARE.");
      break;
    case ITEM_DRUG:
      snprintf(ENDOF(buf), sizeof(buf) - strlen(buf), "It is a dose of ^c%s^n.", drug_types[GET_OBJ_VAL(j, 0)].name);
      break;
    case ITEM_MAGIC_TOOL:
      snprintf(ENDOF(buf), sizeof(buf) - strlen(buf), "It is a ^crating-^c%d %s^n.", GET_OBJ_VAL(j, 1), magic_tool_types[GET_OBJ_VAL(j, 0)]);
      break;
    case ITEM_RADIO:
      snprintf(ENDOF(buf), sizeof(buf) - strlen(buf), "It has a ^c%d/5^n range and can encrypt and decrypt signals up to crypt level ^c%d^n.",
              GET_OBJ_VAL(j, 1), GET_OBJ_VAL(j, 2));
      break;
    case ITEM_GUN_ACCESSORY:
      if (GET_OBJ_VAL(j, 1) == ACCESS_SMARTLINK || GET_OBJ_VAL(j, 1) == ACCESS_GASVENT) {
        snprintf(ENDOF(buf), sizeof(buf) - strlen(buf), "It is a ^crating-%d %s^n that attaches to the ^c%s^n.",
                GET_OBJ_VAL(j, 2), gun_accessory_types[GET_OBJ_VAL(j, 1)], gun_accessory_locations[GET_OBJ_VAL(j, 0)]);
      } else {
        snprintf(ENDOF(buf), sizeof(buf) - strlen(buf), "It is %s ^c%s^n that attaches to the ^c%s^n.",
                AN(gun_accessory_types[GET_OBJ_VAL(j, 1)]), gun_accessory_types[GET_OBJ_VAL(j, 1)]
                , gun_accessory_locations[GET_OBJ_VAL(j, 0)]);
      }
      
      break;
    case ITEM_CLIMBING:
      if (GET_OBJ_VAL(j, 1) == CLIMBING_TYPE_WATER_WINGS) {
        snprintf(ENDOF(buf), sizeof(buf) - strlen(buf), "Despite being classed as 'climbing gear', this is actually a pair of ^cwater wings^n, with rating ^c%d^n.", GET_OBJ_VAL(j, 0));
      } else {
        snprintf(ENDOF(buf), sizeof(buf) - strlen(buf), "Its rating is ^c%d^n.", GET_OBJ_VAL(j, 0));
      }
      break;
    case ITEM_GYRO:
    case ITEM_RCDECK:
      snprintf(ENDOF(buf), sizeof(buf) - strlen(buf), "Its rating is ^c%d^n.", GET_OBJ_VAL(j, 0));
      break;
    case ITEM_CHIP:
      snprintf(ENDOF(buf), sizeof(buf) - strlen(buf), "It grants the skill ^c%s^n at rating ^c%d^n.", skills[GET_CHIP_SKILL(j)].name, GET_CHIP_RATING(j));
      break;
    case ITEM_HOLSTER:
      snprintf(ENDOF(buf), sizeof(buf) - strlen(buf), "It is designed for a ^c%s^n.", holster_types[GET_OBJ_VAL(j, 0)]);
      break;
    case ITEM_DECK_ACCESSORY:
      if (GET_OBJ_VAL(j, 0) == TYPE_FILE) {
        snprintf(ENDOF(buf), sizeof(buf) - strlen(buf), "This file requires ^c%d^n units of space.", GET_DECK_ACCESSORY_FILE_SIZE(j));
      } else if (GET_OBJ_VAL(j, 0) == TYPE_UPGRADE) {
        if (GET_OBJ_VAL(j, 1) == 3) {
          snprintf(ENDOF(buf), sizeof(buf) - strlen(buf), "This cyberdeck upgrade affects ^c%s^n with a rating of ^c%d^n.",
                  deck_accessory_upgrade_types[GET_OBJ_VAL(j, 1)], GET_OBJ_VAL(j, 2));
          if (GET_OBJ_VAL(j, 1) == 5) {
            snprintf(ENDOF(buf), sizeof(buf) - strlen(buf), "\r\nIt is designed for MPCP rating ^c%d^n.", GET_OBJ_VAL(j, 3));
          }
        } else {
          strlcat(buf, "This cyberdeck upgrade adds a ^chitcher jack^n.", sizeof(buf));
        }
      } else if (GET_OBJ_VAL(j, 0) == TYPE_COMPUTER) {
        snprintf(ENDOF(buf), sizeof(buf) - strlen(buf), "This computer has ^c%d^n units of active memory and ^c%d^n units of storage memory.",
                GET_OBJ_VAL(j, 1), GET_OBJ_VAL(j, 2));
      } else if (GET_OBJ_VAL(j, 0) == TYPE_PARTS) {
        snprintf(ENDOF(buf), sizeof(buf) - strlen(buf), "This pack of parts contains ^c%d^n units of ^c%s^n.",
                GET_OBJ_COST(j), GET_OBJ_VAL(j, 1) == 0 ? "general parts" : "memory chips");
      } else if (GET_OBJ_VAL(j, 0) == TYPE_COOKER) {
        snprintf(ENDOF(buf), sizeof(buf) - strlen(buf), "This chip cooker is rating ^c%d^n.", GET_OBJ_VAL(j, 1));
      } else {
        snprintf(buf2, sizeof(buf2), "Error: Unknown ITEM_DECK_ACCESSORY type %d passed to probe command.", GET_OBJ_VAL(j, 0));
        log(buf2);
      }
      break;
    case ITEM_MOD:
      snprintf(ENDOF(buf), sizeof(buf) - strlen(buf), "It is %s ^c%s^n upgrade", AN(mod_types[GET_OBJ_VAL(j, 0)].name), mod_types[GET_OBJ_VAL(j, 0)].name);
      
      // Val 1
      if (GET_OBJ_VAL(j, 0) == TYPE_MOUNT) {
        snprintf(ENDOF(buf), sizeof(buf) - strlen(buf), " that adds %s ^c%s^n.", AN(mount_types[GET_OBJ_VAL(j, 1)]), mount_types[GET_OBJ_VAL(j, 1)]);
      } else {
        snprintf(ENDOF(buf), sizeof(buf) - strlen(buf), " that takes up ^c%d^n load space.", GET_OBJ_VAL(j, 1));
      }
      
      // Val 2
      if (GET_OBJ_VAL(j, 0) == MOD_ENGINE) {
        // engine type
        snprintf(ENDOF(buf), sizeof(buf) - strlen(buf), "\r\nIt is %s ^c%s^n engine.", AN(engine_type[GET_OBJ_VAL(j, 2)]), engine_type[GET_OBJ_VAL(j, 2)]);
      } else if (GET_OBJ_VAL(j, 0) == MOD_RADIO) {
        // radio range 0-5
        snprintf(ENDOF(buf), sizeof(buf) - strlen(buf), "\r\nIt has a ^c%d/5^n range and can encrypt and decrypt signals up to crypt level ^c%d^n.",
                GET_OBJ_VAL(j, 2), GET_OBJ_VAL(j, 3));
      } else {
        snprintf(ENDOF(buf), sizeof(buf) - strlen(buf), "\r\nIt functions at rating ^c%d^n.", GET_OBJ_VAL(j, 2));
      }
      
      // Val 5
      sprintbit(GET_OBJ_VAL(j, 5), engine_type, buf2, sizeof(buf2));
      snprintf(ENDOF(buf), sizeof(buf) - strlen(buf), "\r\nIt is compatible with the following engine types:\r\n^c  %s^n", buf2);
      
      // Vals 4 and 6
      snprintf(ENDOF(buf), sizeof(buf) - strlen(buf), "\r\nIt has been designed to fit ^c%s^n, and installs to the ^c%s^n.",
              GET_OBJ_VAL(j, 4) == 0 ? "vehicles" : GET_OBJ_VAL(j, 4) == 1 ? "drones" : "all types of vehicles",
              mod_name[GET_OBJ_VAL(j, 6)]);
      break;
    case ITEM_DESIGN:
      if (GET_OBJ_VAL(j, 0) == 5) {
        snprintf(ENDOF(buf), sizeof(buf) - strlen(buf), "This design is for a ^crating-%d %s (%s)^n program. It requires ^c%d^n units of storage.\r\n",
                GET_OBJ_VAL(j, 1), programs[GET_OBJ_VAL(j, 0)].name, wound_name[GET_OBJ_VAL(j, 2)],
                (GET_OBJ_VAL(j, 1) * GET_OBJ_VAL(j, 1)) * attack_multiplier[GET_OBJ_VAL(j, 2)]);
      } else {
        snprintf(ENDOF(buf), sizeof(buf) - strlen(buf), "This design is for a ^crating-%d %s^n program. It requires ^c%d^n units of storage.\r\n",
                GET_OBJ_VAL(j, 1), programs[GET_OBJ_VAL(j, 0)].name,
                (GET_OBJ_VAL(j, 1) * GET_OBJ_VAL(j, 1)) * programs[GET_OBJ_VAL(j, 0)].multiplier);
      }
      break;
    case ITEM_GUN_AMMO:
      if (GET_OBJ_VAL(j, 3))
        snprintf(ENDOF(buf), sizeof(buf) - strlen(buf), "It has %d/%d %s round%s of %s ammunition left.\r\n", GET_OBJ_VAL(j, 0), GET_OBJ_VAL(j, 0) +
                     GET_OBJ_VAL(j, 3), ammo_type[GET_OBJ_VAL(j, 2)].name,GET_OBJ_VAL(j, 0) != 1 ? "s" : "",
                     weapon_type[GET_OBJ_VAL(j, 1)]);
      else
        snprintf(ENDOF(buf), sizeof(buf) - strlen(buf), "It has %d %s round%s of %s ammunition left.\r\n", GET_OBJ_VAL(j, 0),
                     ammo_type[GET_OBJ_VAL(j, 2)].name,GET_OBJ_VAL(j, 0) != 1 ? "s" : "",
                     weapon_type[GET_OBJ_VAL(j, 1)]);
      break;
    case ITEM_OTHER:
      if (GET_OBJ_VNUM(j) == OBJ_NEOPHYTE_SUBSIDY_CARD) {
        snprintf(ENDOF(buf), sizeof(buf) - strlen(buf), "It is bonded to %s and has %d nuyen remaining on it.\r\n", 
                GET_IDNUM(ch) == GET_OBJ_VAL(j, 0) ? "you" : "someone else",
                GET_OBJ_VAL(j, 1));
        break;
      }
      // fallthrough
    
    // All info about these is displayed when you examine them.
    case ITEM_GUN_MAGAZINE:
    case ITEM_QUEST:
    case ITEM_CAMERA:
    case ITEM_PHONE:
    case ITEM_KEYRING:
      strlcat(buf, "Nothing stands out about this item's OOC values. Try EXAMINE it instead.", sizeof(buf));
      break;
    default:
      strncpy(buf, "This item type has no probe string. Contact the staff to request one.", sizeof(buf) - strlen(buf));
      break;
  }
  
  if (GET_OBJ_VNUM(j) == OBJ_MULTNOMAH_VISA || GET_OBJ_VNUM(j) == OBJ_CARIBBEAN_VISA) {
    if (GET_OBJ_VAL(j, 0) == GET_IDNUM(ch)) {
      strlcat(buf, "It has your picture on it.", sizeof(buf));
    } else {
      strlcat(buf, "It has someone else's picture on it.", sizeof(buf));
    }
  }
  
  if (GET_OBJ_AFFECT(j).IsSet(AFF_LASER_SIGHT) && has_smartlink) {
    strlcat(buf, "\r\n\r\n^yWARNING:^n Your smartlink overrides your laser sight-- the laser will not function.", sizeof(buf));
  }
  
  strlcat(buf, "^n\r\n\r\n", sizeof(buf));
  found = 0;
  
  if (strcmp(j->obj_flags.bitvector.ToString(), "0") != 0) {
    j->obj_flags.bitvector.PrintBits(buf2, MAX_STRING_LENGTH, affected_bits, AFF_MAX);
    snprintf(ENDOF(buf), sizeof(buf) - strlen(buf), "This object provides the following flags when used: ^c%s^n\r\n", buf2);
  }
  
  // Remove the dont_touch bit for probe printing.
  bool is_dont_touch = IS_OBJ_STAT(j, ITEM_DONT_TOUCH);
  if (is_dont_touch)
    GET_OBJ_EXTRA(j).RemoveBit(ITEM_DONT_TOUCH);
    
  if (strcmp(GET_OBJ_EXTRA(j).ToString(), "0") != 0) {
    GET_OBJ_EXTRA(j).PrintBits(buf2, MAX_STRING_LENGTH, pc_readable_extra_bits, ITEM_EXTRA_MAX);  
    snprintf(ENDOF(buf), sizeof(buf) - strlen(buf), "This object has the following extra features: ^c%s^n\r\n", buf2);
  }
  
  // Restore the dont_touch bit.
  if (is_dont_touch)
    GET_OBJ_EXTRA(j).SetBit(ITEM_DONT_TOUCH);
  
  strncpy(buf1, "This object modifies your character in the following ways when used:\r\n  ^c", sizeof(buf1));
  for (i = 0; i < MAX_OBJ_AFFECT; i++)
    if (j->affected[i].modifier)
    {
      if (GET_OBJ_TYPE(j) == ITEM_MOD)
        sprinttype(j->affected[i].location, veh_aff, buf2, sizeof(buf2));
      else
        sprinttype(j->affected[i].location, apply_types, buf2, sizeof(buf2));
      snprintf(ENDOF(buf1), sizeof(buf1) - strlen(buf1), "%s %+d to %s", found++ ? "," : "",
              j->affected[i].modifier, buf2);
    }
  if (found) {
    snprintf(ENDOF(buf), sizeof(buf) - strlen(buf), "%s^n\r\n", buf1);
  }
  
  if (IS_OBJ_STAT(j, ITEM_NERPS)) {
    strlcat(buf, "^YIt has been flagged NERPS, indicating it has no special coded effects.^n\r\n", sizeof(buf));
  }
  
  if (j->source_info) {
    snprintf(ENDOF(buf), sizeof(buf) - strlen(buf), "\r\nIt references data from the following source book(s): %s^n\r\n", j->source_info);
  }
  
  send_to_char(buf, ch);
}

ACMD(do_examine)
{
  int i, skill = 0;
  struct char_data *tmp_char;
  struct obj_data *tmp_object;
  struct veh_data *found_veh = NULL;
  one_argument(argument, arg);
  
  if (!*arg) {
    send_to_char("Examine what?\r\n", ch);
    return;
  }
  if (subcmd == SCMD_EXAMINE)
    look_at_target(ch, arg);
  
  if (!ch->in_veh || (ch->in_veh && !ch->vfront))
    found_veh = get_veh_list(arg, ch->in_veh ? ch->in_veh->carriedvehs : ch->in_room->vehicles, ch);
    
  if (!found_veh && ch->in_veh)
    found_veh = get_veh_list(arg, (get_ch_in_room(ch))->vehicles, ch);
      
  if (found_veh) {
      if (subcmd == SCMD_PROBE) {
        // If they don't own the vehicle and the hood isn't open, they can't view the stats.
        if (!access_level(ch, LVL_ADMIN) && GET_IDNUM(ch) != found_veh->owner && !found_veh->hood) {
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
  
  // Look at self.
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
      send_to_char("You're not wearing or carrying any such object, and there are no vehicles like that here.\r\n", ch);
    }
    return;
  } else {
    generic_find(arg, FIND_OBJ_INV | FIND_OBJ_ROOM | FIND_CHAR_ROOM |
                 FIND_OBJ_EQUIP | FIND_OBJ_VEH_ROOM | FIND_CHAR_VEH_ROOM, ch, &tmp_char, &tmp_object);
  }
  
  if (tmp_object) {
    if (GET_OBJ_VNUM(tmp_object) == OBJ_NEOPHYTE_SUBSIDY_CARD) {
      send_to_char(ch, "It is bonded to %s and has %d nuyen remaining on it.\r\n", 
              GET_IDNUM(ch) == GET_OBJ_VAL(tmp_object, 0) ? "you" : "someone else",
              GET_OBJ_VAL(tmp_object, 1));
    }
    
#ifdef USE_PRIVATE_CE_WORLD
    display_secret_info_about_object(ch, tmp_object);
#endif
    
    if (GET_OBJ_TYPE(tmp_object) == ITEM_CONTAINER ||
        (GET_OBJ_TYPE(tmp_object) == ITEM_WORN && tmp_object->contains)) {
      if (!tmp_object->contains)
        send_to_char("Looking inside reveals it to be empty.\r\n", ch);
      else {
        float weight_when_full =get_proto_weight(tmp_object) + GET_OBJ_VAL(tmp_object, 0);
        
        if (GET_OBJ_WEIGHT(tmp_object) >= weight_when_full * .9)
          send_to_char("It is bursting at the seams.\r\n", ch);
        else if (GET_OBJ_WEIGHT(tmp_object) >= weight_when_full / 2)
          send_to_char("It is more than half full.\r\n", ch);
        else 
          send_to_char("It is less than half full.\r\n", ch);
        
        send_to_char("When you look inside, you see:\r\n", ch);
        look_in_obj(ch, arg, TRUE);
      }
    } else if ((GET_OBJ_TYPE(tmp_object) == ITEM_WEAPON) &&
               (IS_GUN(GET_OBJ_VAL(tmp_object, 3)))) {
      for (i = 7; i < 10; ++i)
        if (GET_OBJ_VAL(tmp_object, i) >= 0) {
          snprintf(buf1, sizeof(buf1), "There is %s attached to the %s of it.\r\n",
                  GET_OBJ_VAL(tmp_object, i) > 0 ? short_object(GET_OBJ_VAL(tmp_object, i), 2) : "nothing",
                  (i == 7 ? "top" : (i == 8 ? "barrel" : "bottom")));
          send_to_char(buf1, ch);
        }
      if (tmp_object->contains)
        snprintf(buf, sizeof(buf), "It contains %d round%s of %s ammunition, and can hold %d round%s.\r\n",
                MIN(GET_OBJ_VAL(tmp_object->contains, 9), GET_OBJ_VAL(tmp_object, 5)),
                (GET_OBJ_VAL(tmp_object->contains, 9) != 1 ? "s" : ""), ammo_type[GET_OBJ_VAL(tmp_object->contains, 2)].name,
                GET_OBJ_VAL(tmp_object, 5), (GET_OBJ_VAL(tmp_object, 5) != 1 ? "s" : ""));
      else
        snprintf(buf, sizeof(buf), "It does not contain any ammunition, but looks to hold %d round%s.\r\n",
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
      snprintf(buf, sizeof(buf), "The card contains %d nuyen.\r\n", GET_OBJ_VAL(tmp_object, 1));
    else if (GET_OBJ_TYPE(tmp_object) == ITEM_DECK_ACCESSORY && GET_OBJ_VAL(tmp_object, 0) == TYPE_COOKER) {
      if (!tmp_object->contains)
        strncpy(buf, "The small LED is currently off.\r\n", sizeof(buf));
      else if (GET_OBJ_VAL(tmp_object, 9))
        snprintf(buf, sizeof(buf), "The small LED is currently orange, indicating activity. The progress meter for cooking %s is at %d%%.\r\n",
                 GET_OBJ_NAME(tmp_object->contains),
                 (int)(((float)(GET_DECK_ACCESSORY_COOKER_ORIGINAL_TIME(tmp_object) - GET_DECK_ACCESSORY_COOKER_TIME_REMAINING(tmp_object)) / GET_DECK_ACCESSORY_COOKER_ORIGINAL_TIME(tmp_object)) * 100));
      else if (GET_OBJ_TIMER(tmp_object->contains) == -1)
        strncpy(buf, "The small LED is currently flashed red, indicating a failed encode.\r\n", sizeof(buf));
      else
        strncpy(buf, "The small LED is currently green, indicating a successful encode.\r\n", sizeof(buf));
      send_to_char(buf, ch);
    } else if (GET_OBJ_TYPE(tmp_object) == ITEM_PROGRAM && (GET_OBJ_VAL(tmp_object, 0) >= SOFT_ASIST_COLD || GET_OBJ_VAL(tmp_object, 0) < SOFT_SENSOR)) {
      if (GET_OBJ_TIMER(tmp_object) < 0)
        strncpy(buf, "This chip has been ruined.\r\n", sizeof(buf));
      else if (!GET_OBJ_TIMER(tmp_object))
        strncpy(buf, "This program still needs to be encoded to a chip.\r\n", sizeof(buf));
      else if (GET_OBJ_TIMER(tmp_object) > 0)
        strncpy(buf, "This program has been encoded onto an optical chip.\r\n", sizeof(buf));
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
          strlcpy(buf, "Custom Components:\r\n", sizeof(buf));
          for (struct obj_data *soft = tmp_object->contains; soft; soft = soft->next_content)
            if (GET_OBJ_TYPE(soft) == ITEM_PART)
              snprintf(ENDOF(buf), sizeof(buf) - strlen(buf), "%-40s  Type: %-24s  Rating: %d\r\n",
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
        send_to_char("It has not been ^WBOND^ned yet, and ^ywill not function until it is^n.\r\n\r\n", ch);
      }
    }
    if (GET_OBJ_VNUM(tmp_object) > 1) {
      snprintf(buf, sizeof(buf), "You %s that %s ", 
               GET_SKILL(ch, SKILL_POLICE_PROCEDURES) <= 0 ? "have no training in Police Procedures, but you guess" : "think",
               GET_OBJ_NAME(tmp_object));
      int targ = GET_LEGAL_NUM(tmp_object);
      if (targ <= 0)
        strlcat(buf, "is completely legal", sizeof(buf));
      else {
        int skill = get_skill(ch, SKILL_POLICE_PROCEDURES, targ), suc = success_test(skill, targ);
        if (suc <= 0)
          strlcat(buf, "is completely legal", sizeof(buf));
        else {
          if (GET_LEGAL_NUM(tmp_object) <= 3)
            strlcat(buf, "is highly illegal", sizeof(buf));
          else if (GET_LEGAL_NUM(tmp_object) <= 6)
            strlcat(buf, "is illegal", sizeof(buf));
          else strlcat(buf, "could be considered illegal", sizeof(buf));
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
    send_to_char(ch, "You have %ld nuyen.\r\n", GET_NUYEN(ch));
  }
}

ACMD(do_pool)
{
  char pools[MAX_INPUT_LENGTH];
  if (GET_POWER(ch, ADEPT_SIDESTEP))
    snprintf(buf, sizeof(buf), "^R+%d^n", GET_POWER(ch, ADEPT_SIDESTEP));
  else
    strncpy(buf, "  ", sizeof(buf));
  
  if (PRF_FLAGGED(ch, PRF_SCREENREADER)) {
    snprintf(pools, sizeof(pools), "  Dodge: %d%s\r\n  Body: %d\r\n  Offense: %d\r\n  Total Combat Dice: %d\r\n",
            GET_DEFENSE(ch), buf, GET_BODY(ch), GET_OFFENSE(ch), GET_COMBAT(ch));
  } else {
    snprintf(pools, sizeof(pools), "  Combat: %d     (Dodge: %d%s      Body: %d     Offense: %d)\r\n",
            GET_COMBAT(ch), GET_DEFENSE(ch), buf, GET_BODY(ch), GET_OFFENSE(ch));
  }
  if (GET_ASTRAL(ch) > 0)
    snprintf(ENDOF(pools), sizeof(pools) - strlen(pools), "  Astral: %d\r\n", GET_ASTRAL(ch));
  if (GET_HACKING(ch) > 0)
    snprintf(ENDOF(pools), sizeof(pools) - strlen(pools), "  Hacking: %d\r\n", GET_HACKING(ch));
  if (GET_MAGIC(ch) > 0) {
    if (PRF_FLAGGED(ch, PRF_SCREENREADER)) {
      snprintf(ENDOF(pools), sizeof(pools) - strlen(pools), "  Spell: %d\r\n  Casting: %d\r\n  Drain: %d\r\n  Defense: %d\r\n", GET_MAGIC(ch), GET_CASTING(ch), GET_DRAIN(ch), GET_SDEFENSE(ch));
      if (GET_METAMAGIC(ch, META_REFLECTING) == 2)
        snprintf(ENDOF(pools), sizeof(pools) - strlen(pools), "  Reflecting: %d\r\n", GET_REFLECT(ch));
    } else {
      snprintf(ENDOF(pools), sizeof(pools) - strlen(pools), "  Spell: %d      (Casting: %d    Drain: %d    Defense: %d", GET_MAGIC(ch), GET_CASTING(ch), GET_DRAIN(ch), GET_SDEFENSE(ch));
      if (GET_METAMAGIC(ch, META_REFLECTING) == 2)
        snprintf(ENDOF(pools), sizeof(pools) - strlen(pools), "    Reflecting: %d", GET_REFLECT(ch));
      strlcat(pools, ")\r\n", sizeof(pools));
    }
  }
  if (GET_CONTROL(ch) > 0)
    snprintf(ENDOF(pools), sizeof(pools) - strlen(pools), "  Control: %d\r\n", GET_CONTROL(ch));
  for (int x = 0; x < 7; x++)
    if (GET_TASK_POOL(ch, x) > 0)
      snprintf(ENDOF(pools), sizeof(pools) - strlen(pools), "  %s Task Pool: %d\r\n", attributes[x], GET_TASK_POOL(ch, x));
  send_to_char(pools, ch);
}

const char *get_position_string(struct char_data *ch) {
  static char position_string[200];
  
  if (AFF_FLAGGED(ch, AFF_PRONE))
    strlcpy(position_string, "laying prone.", sizeof(position_string));
  else switch (GET_POS(ch)) {
    case POS_DEAD:
      strlcpy(position_string, "DEAD!", sizeof(position_string));
      break;
    case POS_MORTALLYW:
      strlcpy(position_string, "mortally wounded!  You should seek help!", sizeof(position_string));
      break;
    case POS_STUNNED:
      strlcpy(position_string, "stunned!  You can't move!", sizeof(position_string));
      break;
    case POS_SLEEPING:
      strlcpy(position_string, "sleeping.", sizeof(position_string));
      break;
    case POS_RESTING:
      strlcpy(position_string, "resting.", sizeof(position_string));
      break;
    case POS_SITTING:
      strlcpy(position_string, "sitting.", sizeof(position_string));
      break;
    case POS_FIGHTING:
      if (FIGHTING(ch))
        snprintf(position_string, sizeof(position_string), "fighting %s.",PERS(FIGHTING(ch), ch));
      else if (FIGHTING_VEH(ch))
        snprintf(position_string, sizeof(position_string), "fighting %s.", GET_VEH_NAME(FIGHTING_VEH(ch)));
      else
        strlcpy(position_string, "fighting thin air.", sizeof(position_string));
      break;
    case POS_STANDING:
      if (IS_WATER(ch->in_room))
        strlcpy(position_string, "swimming.", sizeof(position_string));
      else
        strlcpy(position_string, "standing.", sizeof(position_string));
      break;
    case POS_LYING:
      strlcpy(position_string, "lying down.", sizeof(position_string));
      break;
    default:
      strlcpy(position_string, "floating.", sizeof(position_string));
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
  
  snprintf(buf2, sizeof(buf2), "Mental condition: %d / %d\r\n", (int)(mental / 100), (int)(GET_MAX_MENTAL(ch) / 100));
  
  snprintf(ENDOF(buf2), sizeof(buf2) - strlen(buf2), "Physical condition: %d / %d\r\n", MAX((int)(physical / 100), 0), (int)(GET_MAX_PHYSICAL(ch) / 100));
  
  if (physical < 0)
    snprintf(ENDOF(buf2), sizeof(buf2) - strlen(buf2), "Physical damage overflow: %d\r\n", (int)(physical / 100) * -1);
  
  return buf2;
}

ACMD(do_hp) {
  send_to_char(get_plaintext_score_health(ch), ch);
}

const char *get_plaintext_score_stats(struct char_data *ch) {
  if (GET_BOD(ch) != GET_REAL_BOD(ch))
    snprintf(buf2, sizeof(buf2), "Body: %d (base body %d)\r\n", GET_BOD(ch), GET_REAL_BOD(ch));
  else
    snprintf(buf2, sizeof(buf2), "Body: %d\r\n", GET_BOD(ch));
  
  if (GET_QUI(ch) != GET_REAL_QUI(ch))
    snprintf(ENDOF(buf2), sizeof(buf2) - strlen(buf2), "Quickness: %d (base quickness %d)\r\n", GET_QUI(ch), GET_REAL_QUI(ch));
  else
    snprintf(ENDOF(buf2), sizeof(buf2) - strlen(buf2), "Quickness: %d\r\n", GET_QUI(ch));
  
  if (GET_STR(ch) != GET_REAL_STR(ch))
    snprintf(ENDOF(buf2), sizeof(buf2) - strlen(buf2), "Strength: %d (base strength %d)\r\n", GET_STR(ch), GET_REAL_STR(ch));
  else
    snprintf(ENDOF(buf2), sizeof(buf2) - strlen(buf2), "Strength: %d\r\n", GET_STR(ch));
  
  if (GET_CHA(ch) != GET_REAL_CHA(ch))
    snprintf(ENDOF(buf2), sizeof(buf2) - strlen(buf2), "Charisma: %d (base charisma %d)\r\n", GET_CHA(ch), GET_REAL_CHA(ch));
  else
    snprintf(ENDOF(buf2), sizeof(buf2) - strlen(buf2), "Charisma: %d\r\n", GET_CHA(ch));
  
  if (GET_INT(ch) != GET_REAL_INT(ch))
    snprintf(ENDOF(buf2), sizeof(buf2) - strlen(buf2), "Intelligence: %d (base intelligence %d)\r\n", GET_INT(ch), GET_REAL_INT(ch));
  else
    snprintf(ENDOF(buf2), sizeof(buf2) - strlen(buf2), "Intelligence: %d\r\n", GET_INT(ch));
  
  if (GET_WIL(ch) != GET_REAL_WIL(ch))
    snprintf(ENDOF(buf2), sizeof(buf2) - strlen(buf2), "Willpower: %d (base willpower %d)\r\n", GET_WIL(ch), GET_REAL_WIL(ch));
  else
    snprintf(ENDOF(buf2), sizeof(buf2) - strlen(buf2), "Willpower: %d\r\n", GET_WIL(ch));
  
  if (GET_TRADITION(ch) == TRAD_MUNDANE)
    strlcat(ENDOF(buf2), "As a Mundane, you have no magic.\r\n", sizeof(buf2));
  else {
    if (GET_MAG(ch) != ch->real_abils.mag)
      snprintf(ENDOF(buf2), sizeof(buf2) - strlen(buf2), "Magic: %d (base magic %d)\r\n", (int)(GET_MAG(ch) / 100), MAX(0, (int)(ch->real_abils.mag / 100)));
    else
      snprintf(ENDOF(buf2), sizeof(buf2) - strlen(buf2), "Magic: %d\r\n", (int)(GET_MAG(ch) / 100));
    
    if (GET_TRADITION(ch) == TRAD_SHAMANIC)
      snprintf(ENDOF(buf2), sizeof(buf2) - strlen(buf2), "You follow %s.\r\n", totem_types[GET_TOTEM(ch)]);
    
    snprintf(ENDOF(buf2), sizeof(buf2) - strlen(buf2), "Initiation grade: %d\r\n", GET_GRADE(ch));
  }
  
  if (GET_REAL_REA(ch) != GET_REA(ch))
    snprintf(ENDOF(buf2), sizeof(buf2) - strlen(buf2), "Effective reaction: %d (real reaction %d)\r\n", GET_REA(ch), GET_REAL_REA(ch));
  else
    snprintf(ENDOF(buf2), sizeof(buf2) - strlen(buf2), "Reaction: %d\r\n", GET_REA(ch));
  
  snprintf(ENDOF(buf2), sizeof(buf2) - strlen(buf2), "Initiative: %d + %dd6\r\n", GET_REA(ch), 1 + GET_INIT_DICE(ch));
  
  return buf2;
}

const char *get_plaintext_score_essence(struct char_data *ch) {
  snprintf(buf2, sizeof(buf2), "Essence: %.2f\r\n", ((float)GET_ESS(ch) / 100));
  snprintf(ENDOF(buf2), sizeof(buf2) - strlen(buf2), "Bioware Index: %.2f\r\n", ((float)GET_INDEX(ch) / 100));
  snprintf(ENDOF(buf2), sizeof(buf2) - strlen(buf2), "Essence Index: %.2f\r\n", ((float)GET_ESS(ch) / 100) + 3);
  return buf2;
}

const char *get_plaintext_score_equipment(struct char_data *ch) {
  snprintf(buf2, sizeof(buf2), "Armor: %d ballistic, %d impact\r\n", GET_BALLISTIC(ch), GET_IMPACT(ch));
  snprintf(ENDOF(buf2), sizeof(buf2) - strlen(buf2), "Nuyen: %ld\r\n", GET_NUYEN(ch));
  snprintf(ENDOF(buf2), sizeof(buf2) - strlen(buf2), "You are carrying %.2f kilos. Your maximum carry weight is %d.\r\n", IS_CARRYING_W(ch), CAN_CARRY_W(ch));
  return buf2;
}

const char *get_plaintext_score_karma(struct char_data *ch) {
  snprintf(buf2, sizeof(buf2), "Current karma: %.2f\r\n", ((float)GET_KARMA(ch) / 100));
  snprintf(ENDOF(buf2), sizeof(buf2) - strlen(buf2), "Total karma earned: %.2f\r\n", ((float)GET_TKE(ch)));
  snprintf(ENDOF(buf2), sizeof(buf2) - strlen(buf2), "Reputation: %d\r\n", GET_REP(ch));
  snprintf(ENDOF(buf2), sizeof(buf2) - strlen(buf2), "Notoriety: %d\r\n", GET_NOT(ch));
  return buf2;
}

const char *get_plaintext_score_misc(struct char_data *ch) {
  snprintf(buf2, sizeof(buf2), "You are %s\r\n", get_position_string(ch));
  
  if (ch->desc != NULL && ch->desc->original != NULL ) {
    if (PLR_FLAGGED(ch->desc->original, PLR_MATRIX))
      snprintf(ENDOF(buf2), sizeof(buf2) - strlen(buf2), "You are connected to the Matrix.\r\n");
    else if (IS_PROJECT(ch))
      snprintf(ENDOF(buf2), sizeof(buf2) - strlen(buf2), "You are astrally projecting.\r\n");
    else
      snprintf(ENDOF(buf2), sizeof(buf2) - strlen(buf2), "You are occupying the body of %s.\r\n", GET_NAME(ch));
  }
  
  strlcat(buf2, get_vision_string(ch), sizeof(buf2));
  
  if (IS_AFFECTED(ch, AFF_INVISIBLE) || IS_AFFECTED(ch, AFF_IMP_INVIS) || IS_AFFECTED(ch, AFF_SPELLINVIS) || IS_AFFECTED(ch, AFF_SPELLIMPINVIS))
    strlcat(buf2, "You are invisible.\r\n", sizeof(buf2));
  
#ifdef ENABLE_HUNGER
  if (GET_COND(ch, COND_FULL) == 0)
    strlcat(buf2, "You are hungry.\r\n", sizeof(buf2));
  
  if (GET_COND(ch, COND_THIRST) == 0)
    strlcat(buf2, "You are thirsty.\r\n", sizeof(buf2));
#endif
  
  if (GET_COND(ch, COND_DRUNK) > 10)
    strlcat(buf2, "You are intoxicated.\r\n", sizeof(buf2));
  
  if (AFF_FLAGGED(ch, AFF_SNEAK))
    snprintf(ENDOF(buf2), sizeof(buf2) - strlen(buf2), "You are sneaking.\r\n");
    
  snprintf(ENDOF(buf2), sizeof(buf2) - strlen(buf2), "You have accrued %d out of a possible %d socialization bonus points.\r\n",
           GET_CONGREGATION_BONUS(ch), MAX_CONGREGATION_BONUS);
  
  // Physical and misc attributes.
  snprintf(ENDOF(buf2), sizeof(buf2) - strlen(buf2), "Height: %.2f meters\r\n", ((float)GET_HEIGHT(ch) / 100));
  snprintf(ENDOF(buf2), sizeof(buf2) - strlen(buf2), "Weight: %d kilos\r\n", GET_WEIGHT(ch));
  
  
  struct time_info_data playing_time;
  struct time_info_data real_time_passed(time_t t2, time_t t1);
  playing_time = real_time_passed(time(0) + ch->player.time.played, ch->player.time.logon);
  
  snprintf(ENDOF(buf2), sizeof(buf2) - strlen(buf2), "Current session length: %d days, %d hours.\r\n", playing_time.day, playing_time.hours);
  
  return buf2;
}

const char *get_plaintext_score_combat(struct char_data *ch) {
  strlcpy(buf3, get_plaintext_score_health(ch), sizeof(buf3));
  
  snprintf(ENDOF(buf3), sizeof(buf3) - strlen(buf3), "Initiative: %d + %dd6\r\n", GET_REA(ch), 1 + GET_INIT_DICE(ch));
  
  snprintf(ENDOF(buf3), sizeof(buf3) - strlen(buf3), "Armor: %d ballistic, %d impact\r\n", GET_BALLISTIC(ch), GET_IMPACT(ch));
  
  strlcat(buf3, get_vision_string(ch), sizeof(buf3));
  
  snprintf(ENDOF(buf3), sizeof(buf3) - strlen(buf3), "You are %s\r\n", get_position_string(ch));
  
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
      snprintf(screenreader_buf, sizeof(screenreader_buf),
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
      snprintf(buf, sizeof(buf), "You are rigging %s.\r\n", GET_VEH_NAME(veh));
      snprintf(ENDOF(buf), sizeof(buf) - strlen(buf), "Damage:^R%3d/10^n      Mental:^B%3d(%2d)^n\r\n"
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
    
    strncpy(buf, "^b//^L//^b//^L//^b//^L//^b//^L//^b//^L//^b//^L//^b//^L//^b//^L//^b//^L//^b//^L//"
                 "^b//^L//^b//^L//^b//^L//^b//^L^L//^b//^L//^b//^L//^b//^L//^b//^L/\r\n"
                 "^b/^L/  ^L \\_\\                                 ^rcondition monitor           ^L/^b/\r\n", sizeof(buf));
    
    snprintf(screenreader_buf, sizeof(screenreader_buf), "Score sheet for %s:\r\n", GET_CHAR_NAME(ch));
    
    strlcat(buf, "^L/^b/^L `//-\\\\                      ^gMent: ", sizeof(buf));
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
      strlcat(buf, " ^YMasked by pain editor.      ^b/^L/\r\n", sizeof(buf));
    } else {
      if (mental >= 900 && mental < 1000)
        strlcat(buf, "^b[^R*^b]", sizeof(buf));
      else
        strlcat(buf, "^b[^gL^b]", sizeof(buf));
      if (mental >= 800 && mental < 900)
        strlcat(buf, "^b[^R*^b]", sizeof(buf));
      else
        strlcat(buf, "^b[ ]", sizeof(buf));
      if (mental >= 700 && mental < 800)
        strlcat(buf, "^b[^R*^b]", sizeof(buf));
      else
        strlcat(buf, "^b[^yM^b]", sizeof(buf));
      if (mental >= 600 && mental < 700)
        strlcat(buf, "^b[^R*^b]", sizeof(buf));
      else
        strlcat(buf, "^b[ ]", sizeof(buf));
      if (mental >= 500 && mental < 600)
        strlcat(buf, "^b[^R*^b]", sizeof(buf));
      else
        strlcat(buf, "^b[ ]", sizeof(buf));
      if (mental >= 400 && mental < 500)
        strlcat(buf, "^b[^R*^b]", sizeof(buf));
      else
        strlcat(buf, "^b[^rS^b]", sizeof(buf));
      if (mental >= 300 && mental < 400)
        strlcat(buf, "^b[^R*^b]", sizeof(buf));
      else
        strlcat(buf, "^b[ ]", sizeof(buf));
      if (mental >= 200 && mental < 300)
        strlcat(buf, "^b[^R*^b]", sizeof(buf));
      else
        strlcat(buf, "^b[ ]", sizeof(buf));
      if (mental >= 100 && mental < 200)
        strlcat(buf, "^b[^R*^b]", sizeof(buf));
      else
        strlcat(buf, "^b[ ]", sizeof(buf));
      if (mental < 100)
        strlcat(buf, "^b[^R*^b]", sizeof(buf));
      else
        strlcat(buf, "^b[^RD^b]", sizeof(buf));
      strlcat(buf, "  ^b/^L/\r\n", sizeof(buf));
    }
      
    strlcat(buf, "^b/^L/ ^L`\\\\-\\^wHADOWRUN 3rd Edition   ^rPhys: ", sizeof(buf));
    if (pain_editor) {
      strlcat(buf, " ^YMasked by pain editor.        ^L/^b/\r\n", sizeof(buf));
    } else {
      if (physical >= 900 && physical < 1000)
        strlcat(buf, "^L[^R*^L]", sizeof(buf));
      else
        strlcat(buf, "^L[^gL^L]", sizeof(buf));
      if (physical >= 800 && physical < 900)
        strlcat(buf, "^L[^R*^L]", sizeof(buf));
      else
        strlcat(buf, "^L[ ]", sizeof(buf));
      if (physical >= 700 && physical < 800)
        strlcat(buf, "^L[^R*^L]", sizeof(buf));
      else
        strlcat(buf, "^L[^yM^L]", sizeof(buf));
      if (physical >= 600 && physical < 700)
        strlcat(buf, "^L[^R*^L]", sizeof(buf));
      else
        strlcat(buf, "^L[ ]", sizeof(buf));
      if (physical >= 500 && physical < 600)
        strlcat(buf, "^L[^R*^L]", sizeof(buf));
      else
        strlcat(buf, "^L[ ]", sizeof(buf));
      if (physical >= 400 && physical < 500)
        strlcat(buf, "^L[^R*^L]", sizeof(buf));
      else
        strlcat(buf, "^L[^rS^L]", sizeof(buf));
      if (physical >= 300 && physical < 400)
        strlcat(buf, "^L[^R*^L]", sizeof(buf));
      else
        strlcat(buf, "^L[ ]", sizeof(buf));
      if (physical >= 200 && physical < 300)
        strlcat(buf, "^L[^R*^L]", sizeof(buf));
      else
        strlcat(buf, "^L[ ]", sizeof(buf));
      if (physical >= 100 && physical < 200)
        strlcat(buf, "^L[^R*^L]", sizeof(buf));
      else
        strlcat(buf, "^L[ ]", sizeof(buf));
      if (physical < 100)
        strlcat(buf, "^L[^R*^L]", sizeof(buf));
      else
        strlcat(buf, "^L[^RD^L]", sizeof(buf));
      strlcat(buf, "  ^L/^b/\r\n", sizeof(buf));
    }
    
    if (pain_editor) {
      strlcat(buf, "^L/^b/  ^L///-\\  ^wcharacter sheet                                           ^b/^L/\r\n", sizeof(buf));
    } else {
      strlcat(buf, "^L/^b/  ^L///-\\  ^wcharacter sheet           ^LPhysical Damage Overflow: ", sizeof(buf));
      if (physical < 0)
        snprintf(ENDOF(buf), sizeof(buf) - strlen(buf), "^R[%2d]  ^b/^L/\r\n", (int)(physical / 100) * -1);
      else
        strlcat(buf, "^R[ 0]  ^b/^L/\r\n", sizeof(buf));
    }
    
    /* Calculate the various bits of data that the score sheet needs. */
    
    playing_time = real_time_passed(time(0) + ch->player.time.played, ch->player.time.logon);
    
    char out_of_body_string[200];
    if (ch->desc != NULL && ch->desc->original != NULL ) {
      if (PLR_FLAGGED(ch->desc->original, PLR_MATRIX))
        snprintf(out_of_body_string, sizeof(out_of_body_string), "You are connected to the Matrix.");
      else if (IS_PROJECT(ch))
        snprintf(out_of_body_string, sizeof(out_of_body_string), "You are astrally projecting.");
      else
        snprintf(out_of_body_string, sizeof(out_of_body_string), "You are occupying the body of %s.", GET_NAME(ch));
    } else {
      strlcpy(out_of_body_string, "", sizeof(out_of_body_string));
    }
    
    static char invisibility_string[50];
    if (IS_AFFECTED(ch, AFF_INVISIBLE) || IS_AFFECTED(ch, AFF_IMP_INVIS) || IS_AFFECTED(ch, AFF_SPELLINVIS) || IS_AFFECTED(ch, AFF_SPELLIMPINVIS))
      strlcpy(invisibility_string, "You are invisible.", sizeof(invisibility_string));
    else
      strlcpy(invisibility_string, "", sizeof(invisibility_string));
    
    
    char shaman_string[50];
    if (GET_TRADITION(ch) == TRAD_SHAMANIC)
      snprintf(shaman_string, sizeof(shaman_string), "You follow %s.", totem_types[GET_TOTEM(ch)]);
    else
      strlcpy(shaman_string, "", sizeof(shaman_string));
    
    static char grade_string[50];
    if (GET_TRADITION(ch) != TRAD_MUNDANE)
      snprintf(grade_string, sizeof(grade_string), "^nGrade: ^w[^W%2d^w]", GET_GRADE(ch));
    else
      strlcpy(grade_string, "", sizeof(grade_string));
    
    snprintf(ENDOF(buf), sizeof(buf) - strlen(buf), "^b/^L/  ^L\\\\@//                                                            ^L/^b/\r\n");
    snprintf(ENDOF(buf), sizeof(buf) - strlen(buf), "^L/^b/   ^L`^^                                                              ^b/^L/\r\n");
    snprintf(ENDOF(buf), sizeof(buf) - strlen(buf), "^b/^L/                                                                   ^L/^b/\r\n");
    snprintf(ENDOF(buf), sizeof(buf) - strlen(buf), "^L/^b/ ^nBody          ^w%2d (^W%2d^w)    Height: ^W%4.2f^w meters   Weight: ^W%3d^w kilos  ^b/^L/\r\n",
                          GET_REAL_BOD(ch), GET_BOD(ch), ((float)GET_HEIGHT(ch) / 100), GET_WEIGHT(ch));
    snprintf(ENDOF(buf), sizeof(buf) - strlen(buf), "^b/^L/ ^nQuickness     ^w%2d (^W%2d^w)    Encumbrance: ^W%6.2f^w kgs carried, ^W%3d^w max ^L/^b/\r\n",
                          GET_REAL_QUI(ch), GET_QUI(ch), IS_CARRYING_W(ch) ,CAN_CARRY_W(ch));
    snprintf(ENDOF(buf), sizeof(buf) - strlen(buf), "^L/^b/ ^nStrength      ^w%2d (^W%2d^w)    Current session length ^W%2d^w days, ^W%2d^w hours.^b/^L/\r\n",
                          GET_REAL_STR(ch), GET_STR(ch), playing_time.day, playing_time.hours);
    snprintf(ENDOF(buf), sizeof(buf) - strlen(buf), "^b/^L/ ^nCharisma      ^w%2d (^W%2d^w)    ^wKarma ^B[^W%7.2f^B] ^wRep ^B[^W%4d^B] ^rNotor ^r[^R%4d^r]  ^L/^b/\r\n",
                          GET_REAL_CHA(ch), GET_CHA(ch), ((float)GET_KARMA(ch) / 100), GET_REP(ch), GET_NOT(ch));
    snprintf(ENDOF(buf), sizeof(buf) - strlen(buf), "^L/^b/ ^nIntelligence  ^w%2d (^W%2d^w)    ^r%-41s^b/^L/\r\n",
                          GET_REAL_INT(ch), GET_INT(ch), get_vision_string(ch, TRUE));
    snprintf(ENDOF(buf), sizeof(buf) - strlen(buf), "^b/^L/ ^nWillpower     ^w%2d (^W%2d^w)    ^nYou are %-33s^L/^b/\r\n",
                          GET_REAL_WIL(ch), GET_WIL(ch), get_position_string(ch));
    snprintf(ENDOF(buf), sizeof(buf) - strlen(buf), "^L/^b/ ^nEssence       ^g[^w%5.2f^g]    ^W%-18s                       ^b/^L/\r\n",
                          ((float)GET_ESS(ch) / 100), invisibility_string);
#ifdef ENABLE_HUNGER
    snprintf(ENDOF(buf), sizeof(buf) - strlen(buf), "^b/^L/ ^nBioware Index ^B[^w%5.2f^B]    ^n%-15s                          ^L/^b/\r\n",
                          ((float)GET_INDEX(ch) / 100), GET_COND(ch, COND_FULL) == 0 ? "You are hungry." : "");
    snprintf(ENDOF(buf), sizeof(buf) - strlen(buf), "^L/^b/ ^nEssence Index ^W[^w%5.2f^W]    ^n%-16s                         ^b/^L/\r\n",
                          (((float)GET_ESS(ch) / 100) + 3), GET_COND(ch, COND_THIRST) == 0 ? "You are thirsty." : "");
#else
    snprintf(ENDOF(buf), sizeof(buf) - strlen(buf), "^b/^L/ ^nBioware Index ^B[^w%5.2f^B]    ^n%-15s                          ^L/^b/\r\n",
                          ((float)GET_INDEX(ch) / 100), "");
    snprintf(ENDOF(buf), sizeof(buf) - strlen(buf), "^L/^b/ ^nEssence Index ^W[^w%5.2f^W]    ^n%-16s                         ^b/^L/\r\n",
                          (((float)GET_ESS(ch) / 100) + 3), "");
#endif
    snprintf(ENDOF(buf), sizeof(buf) - strlen(buf), "^b/^L/ ^nMagic         ^w%2d (^W%2d^w)    ^g%-20s                     ^L/^b/\r\n",
                          MAX(0, ((int)ch->real_abils.mag / 100)), ((int)GET_MAG(ch) / 100), GET_COND(ch, COND_DRUNK) > 10 ? "You are intoxicated." : "");
    snprintf(ENDOF(buf), sizeof(buf) - strlen(buf), "^L/^b/ ^nReaction      ^w%2d (^W%2d^w)    ^c%-41s^b/^L/\r\n",
                          GET_REAL_REA(ch), GET_REA(ch), out_of_body_string);
    snprintf(ENDOF(buf), sizeof(buf) - strlen(buf), "^b/^L/ ^nInitiative^w   [^W%2d^w+^W%d^rd6^n]    ^n%-32s         ^L/^b/\r\n",
                          GET_REA(ch), 1 + GET_INIT_DICE(ch), shaman_string);
    snprintf(ENDOF(buf), sizeof(buf) - strlen(buf), "^L/^b/ ^nArmor     ^w[ ^W%2d^rB^w/ ^W%2d^rI^w]    ^L%-17s^n                        ^b/^L/\r\n",
                          GET_BALLISTIC(ch), GET_IMPACT(ch), AFF_FLAGGED(ch, AFF_SNEAK) ? "You are sneaking." : "");
    snprintf(ENDOF(buf), sizeof(buf) - strlen(buf), "^b/^L/ ^nNuyen     ^w[^W%'9ld^w]    %11s   Social Bonus: [^W%3d / %3d^n]  ^L/^b/\r\n",
                          GET_NUYEN(ch), grade_string, GET_CONGREGATION_BONUS(ch), MAX_CONGREGATION_BONUS);
    strlcat(buf, "^L/^b/                                                                   ^b/^L/\r\n"
            "^L//^b//^L//^b//^L//^b//^L//^b//^L//^b//^L//^b//^L//^b//"
            "^L//^b//^L//^b//^L//^b//^L//^b//^L//^b//^L//^b//^L//"
            "^b//^L^L//^b//^L//^b//^L//^b//^L//^b/\r\n", sizeof(buf));
    
    // Compose the remainder of the screenreader score.
    
    // Health:
    strlcat(screenreader_buf, get_plaintext_score_health(ch), sizeof(screenreader_buf));
    
    // Attributes:
    strlcat(screenreader_buf, get_plaintext_score_stats(ch), sizeof(screenreader_buf));
    
    // Derived stats:
    strlcat(screenreader_buf, get_plaintext_score_essence(ch), sizeof(screenreader_buf));
    
    // Equipment attributes:
    strlcat(screenreader_buf, get_plaintext_score_equipment(ch), sizeof(screenreader_buf));
    
    // Karma stats.
    strlcat(screenreader_buf, get_plaintext_score_karma(ch), sizeof(screenreader_buf));
    
    // Condition and misc affects.
    strlcat(screenreader_buf, get_plaintext_score_misc(ch), sizeof(screenreader_buf));
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
  
  float ammo_weight = get_bulletpants_weight(ch);
  if (ammo_weight > 0)
    send_to_char(ch, "\r\nAdditionally, you have %.2f kilos of ammo in your pockets.\r\n", ammo_weight);
  else
    send_to_char("\r\nYou have no ammo in your pockets.\r\n", ch);
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
    snprintf(buf, sizeof(buf), "%-40s Essence: %0.2f\r\n",
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
    snprintf(buf, sizeof(buf), "%-40s Rating: %-2d     Body Index: %0.2f\r\n",
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
  char not_const[MAX_INPUT_LENGTH];
  strlcpy(not_const, argument, sizeof(not_const));
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
    snprintf(buf, sizeof(buf), "%d o'clock %s, %s, %s %d, %d.\r\n", hour, pm ? "PM" : "AM",
            weekdays[(int)time_info.weekday], month_name[month - 1], day, time_info.year);
  else
    snprintf(buf, sizeof(buf), "%d:%s%d %s, %s, %s%d/%s%d/%d.\r\n", hour,
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
    snprintf(buf, sizeof(buf), "The sky is %s and %s.\r\n", sky_look[weather_info.sky],
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
    snprintf(query, sizeof(query), "SELECT name FROM help_topic ORDER BY name ASC");
  } else {
    char letter = *temp;
    if (!isalpha(letter)) {
      send_to_char("Only letters can be sent to the index command.\r\n", ch);
      return;
    }
    
    letter = tolower(letter);
    
    // No prepare_quotes since this is guaranteed to be a single alphanumeric character.
    snprintf(query, sizeof(query), "SELECT name FROM help_topic WHERE name LIKE '%c%%'", letter);
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

void display_help(char *help, int help_len, const char *arg, struct char_data *ch) {
  char query[MAX_STRING_LENGTH];
  MYSQL_RES *res;
  MYSQL_ROW row;
  *help = '\0';
  
  // Buf now holds the quoted version of arg.
  prepare_quotes(buf, arg, sizeof(buf) / sizeof(buf[0]));
  
  // First strategy: Look for an exact match.
  snprintf(query, sizeof(query), "SELECT * FROM help_topic WHERE name='%s'", buf);
  if (mysql_wrapper(mysql, query)) {
    // We got a SQL error-- bail.
    snprintf(help, help_len, "The help system is temporarily unavailable.\r\n");
    snprintf(buf3, sizeof(buf3), "WARNING: Failed to return help topic about %s. See server log for MYSQL error.", arg);
    mudlog(buf3, ch, LOG_SYSLOG, TRUE);
    return;
  } else {
    res = mysql_store_result(mysql);
    if ((row = mysql_fetch_row(res))) {
      // Found it-- send it back and have done with it.
      snprintf(help, help_len, "^W%s^n\r\n\r\n%s\r\n", row[0], row[1]);
      mysql_free_result(res);
      return;
    } else {
      // Failed to find a match-- clean up and continue.
      mysql_free_result(res);
    }
  }
  
  // Second strategy: Search for possible like-matches.
  snprintf(query, sizeof(query), "SELECT * FROM help_topic WHERE name LIKE '%%%s%%' ORDER BY name ASC", buf);
  if (mysql_wrapper(mysql, query)) {
    // If we don't find it here either, we know the file doesn't exist-- failure condition.
    snprintf(help, help_len, "No such help file exists.\r\n");
    return;
  }
  res = mysql_store_result(mysql);
  int x = mysql_num_rows(res);
  
  // If we have no rows, fail.
  if (x < 1) {
    snprintf(help, help_len, "No such help file exists.\r\n");
    snprintf(buf3, sizeof(buf3), "Failed helpfile search: %s.", arg);
    mudlog(buf3, ch, LOG_HELPLOG, TRUE);
    mysql_free_result(res);
    return;
  }
  
  // If we have too many rows, try to refine the search to just files starting with the search string.
  else if (x > 5) {
    // Prepare a just-in-case response with the topics that were found in the overbroad search.
    snprintf(help, help_len, "%d articles returned, please narrow your search. The topics found were:\r\n", x);
    while ((row = mysql_fetch_row(res))) {
      snprintf(ENDOF(help), help_len - strlen(help), "^W%s^n\r\n", row[0]);
    }
    mysql_free_result(res);
    
    // Try a lookup with just files that have the search string at the start of their title.
    snprintf(query, sizeof(query), "SELECT * FROM help_topic WHERE name LIKE '%s%%' ORDER BY name ASC", buf);
    if (mysql_wrapper(mysql, query)) {
      // We hit an error with this followup search, so we just return our pre-prepared string with the search that succeeded.
      snprintf(buf3, sizeof(buf3), "Overbroad helpfile search combined with follow-up lookup failure (%d articles): %s.", x, arg);
      mudlog(buf3, ch, LOG_HELPLOG, TRUE);
      return;
    }
    res = mysql_store_result(mysql);
    row = mysql_fetch_row(res);
    if (mysql_num_rows(res) == 1) {
      // We found a single candidate row-- send that back.
      snprintf(help, help_len, "^W%s^n\r\n\r\n%s\r\n", row[0], row[1]);
    } else {
      // We didn't find any candidate rows, or we found too many. They'll get the pre-prepared string, and we'll log this one.
      snprintf(buf3, sizeof(buf3), "Overbroad helpfile search (%d articles): %s.", x, arg);
      mudlog(buf3, ch, LOG_HELPLOG, TRUE);
    }
    mysql_free_result(res);
    return;
  }
  
  // We found 1-5 results. Chunk 'em together and return them.
  else {
    while ((row = mysql_fetch_row(res))) {
      snprintf(ENDOF(help), help_len - strlen(help), "^W%s^n\r\n\r\n%s\r\n", row[0], row[1]);
    }
    mysql_free_result(res);
    return;
  }
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
  
  display_help(buf, MAX_STRING_LENGTH*2, argument, ch);
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
    snprintf(buf, sizeof(buf), "The following privileged commands are available to %s:\r\n",
            vict == ch ? "you" : GET_CHAR_NAME(vict));
    
    /* cmd_num starts at 1, not 0, to remove 'RESERVED' */
    for (no = 1, cmd_num = 1; *cmd_info[cmd_num].command != '\n'; cmd_num++)
      if (cmd_info[cmd_num].minimum_level >= LVL_BUILDER
          && GET_LEVEL(vict) >= cmd_info[cmd_num].minimum_level) {
        snprintf(ENDOF(buf), sizeof(buf) - strlen(buf), "%-13s", cmd_info[cmd_num].command);
        if (!(no % 6))
          strlcat(buf, "\r\n", sizeof(buf));
        no++;
      }
    strlcat(buf, "\r\n", sizeof(buf));
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
   snprintf(buf, sizeof(buf), "The following privileged commands are available to %s:\r\n",
   vict == ch ? "you" : GET_CHAR_NAME(vict));
   
   for (no = 1, cmd_num = 1; *cmd_info[cmd_num].command != '\n'; cmd_num++)
   if (cmd_info[cmd_num].minimum_level >= LVL_BUILDER
   && GET_LEVEL(vict) >= cmd_info[cmd_num].minimum_level) {
   snprintf(ENDOF(buf), sizeof(buf) - strlen(buf), "%-13s", cmd_info[cmd_num].command);
   if (!(no % 6))
   strlcat(buf, "\r\n", sizeof(buf));
   no++;
   }
   strlcat(buf, "\r\n", sizeof(buf));
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
  char not_const[MAX_INPUT_LENGTH];
  strlcpy(not_const, argument, sizeof(not_const));
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
  strlcpy(buf, argument, sizeof(buf));
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
    strlcpy(buf, buf1, sizeof(buf));
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
      if (ooc && (PRF_FLAGGED(tch, PRF_NOOOC) || PLR_FLAGGED(tch, PLR_NOT_YET_AUTHED)))
        continue;
      if (newbie && !PLR_FLAGGED(tch, PLR_NEWBIE))
        continue;
      if (GET_INCOG_LEV(tch) > GET_LEVEL(ch))
        continue;
      num_can_see++;
      
      if ( output_header ) {
        output_header = 0;
        if ( sort >= LVL_BUILDER )
          strlcat(buf, "\r\n^W Immortals ^L: ^WStaff Online\r\n ^w---------------------------\r\n", sizeof(buf));
        else
          strlcat(buf, "\r\n     ^W Race ^L: ^WVisible Players\r\n ^W---------------------------\r\n", sizeof(buf));
      }
      
      switch (GET_LEVEL(tch)) {
        case LVL_BUILDER:
        case LVL_ARCHITECT:
          snprintf(buf1, sizeof(buf1), "^G");
          break;
        case LVL_FIXER:
        case LVL_CONSPIRATOR:
          snprintf(buf1, sizeof(buf1), "^m");
          break;
        case LVL_EXECUTIVE:
          snprintf(buf1, sizeof(buf1), "^c");
          break;
        case LVL_DEVELOPER:
          snprintf(buf1, sizeof(buf1), "^r");
          break;
        case LVL_VICEPRES:
        case LVL_ADMIN:
          snprintf(buf1, sizeof(buf1), "^b");
          break;
        case LVL_PRESIDENT:
          snprintf(buf1, sizeof(buf1), "^B");
          break;
        default:
          snprintf(buf1, sizeof(buf1), "^L");
          break;
      }
      if (PRF_FLAGGED(tch, PRF_SHOWGROUPTAG) && GET_PGROUP_MEMBER_DATA(tch) && GET_PGROUP(tch)) {
        snprintf(buf2, sizeof(buf2), "%10s :^N %s%s^N%s%s%s %s^n",
                (GET_WHOTITLE(tch) ? GET_WHOTITLE(tch) : ""),
                (GET_PGROUP(tch)->get_tag()),
                (strlen(GET_PGROUP(tch)->get_tag()) ? " " : ""),
                (GET_PRETITLE(tch) ? GET_PRETITLE(tch) : ""),
                (GET_PRETITLE(tch) && strlen(GET_PRETITLE(tch)) ? " " : ""),
                GET_CHAR_NAME(tch),
                GET_TITLE(tch));
      } else {
        snprintf(buf2, sizeof(buf2), "%10s :^N %s%s%s %s^n",
                (GET_WHOTITLE(tch) ? GET_WHOTITLE(tch) : ""),
                (GET_PRETITLE(tch) ? GET_PRETITLE(tch) : ""),
                (GET_PRETITLE(tch) && strlen(GET_PRETITLE(tch)) ? " " : ""),
                GET_CHAR_NAME(tch),
                GET_TITLE(tch));
      }
      strlcat(buf1, buf2, sizeof(buf1));
      
      if (GET_PGROUP_MEMBER_DATA(tch) 
          && GET_PGROUP(tch)
          && (access_level(ch, LVL_ADMIN) 
              || (!GET_PGROUP(tch)->is_secret()
                  && GET_PGROUP_MEMBER_DATA(ch) 
                  && GET_PGROUP(ch) == GET_PGROUP(tch)))) 
      {
        snprintf(buf2, sizeof(buf2), " ^G(^g%s^G)^n", GET_PGROUP(tch)->get_alias());
        strlcat(buf1, buf2, sizeof(buf1));
      }
      
      if (PRF_FLAGGED(tch, PRF_AFK))
        strlcat(buf1, " (AFK)", sizeof(buf1));
      if (PLR_FLAGGED(tch, PLR_RPE) && (level > LVL_MORTAL || PLR_FLAGGED(ch, PLR_RPE)))
        strlcat(buf1, " ^R(RPE)^n", sizeof(buf1));
      
      if (PRF_FLAGGED(tch, PRF_NEWBIEHELPER) && (level > LVL_MORTAL || PRF_FLAGGED(ch, PRF_NEWBIEHELPER)))
        strlcat(buf1, " ^G(Newbie Helper)^n", sizeof(buf1));
      
      if (level > LVL_MORTAL) {
        if (GET_INVIS_LEV(tch) && level >= GET_INVIS_LEV(tch))
          snprintf(ENDOF(buf1), sizeof(buf1) - strlen(buf1), " (i%d)", GET_INVIS_LEV(tch));
        if (GET_INCOG_LEV(tch))
          snprintf(ENDOF(buf1), sizeof(buf1) - strlen(buf1), " (c%d)", GET_INCOG_LEV(tch));
        if (PLR_FLAGGED(tch, PLR_MAILING))
          strlcat(buf1, " (mailing)", sizeof(buf1));
        else if (PLR_FLAGGED(tch, PLR_WRITING))
          strlcat(buf1, " (writing)", sizeof(buf1));
        if (PLR_FLAGGED(tch, PLR_EDITING))
          strlcat(buf1, " (editing)", sizeof(buf1));
        if (PRF_FLAGGED(tch, PRF_QUESTOR))
          strlcat(buf1, " ^Y(questor)^n", sizeof(buf1));
        if (PLR_FLAGGED(tch, PLR_NOT_YET_AUTHED))
          strlcat(buf1, " ^G(unauthed)^n", sizeof(buf1));
        if (PLR_FLAGGED(tch, PLR_MATRIX))
          strlcat(buf1, " (decking)", sizeof(buf1));
        else if (PLR_FLAGGED(tch, PLR_PROJECT))
          strlcat(buf1, " (projecting)", sizeof(buf1));
        else if (d->original) {
          if (IS_NPC(d->character) && GET_MOB_VNUM(d->character) >= 50 &&
              GET_MOB_VNUM(d->character) < 70)
            strlcat(buf1, " (morphed)", sizeof(buf1));
          else
            strlcat(buf1, " (switched)", sizeof(buf1));
        }
      }
      if (!quest && PRF_FLAGGED(tch, PRF_QUEST))
        strlcat(buf1, " ^Y(hired)^n", sizeof(buf1));
      if (PRF_FLAGGED(tch, PRF_NOTELL))
        strlcat(buf1, " (!tell)", sizeof(buf1));
      if (PLR_FLAGGED(tch, PLR_KILLER))
        strlcat(buf1, " ^R(KILLER)^N", sizeof(buf1));
      if (PLR_FLAGGED(tch, PLR_BLACKLIST))
        strlcat(buf1, " ^L(BLACKLISTED)^N", sizeof(buf1));
      if (PLR_FLAGGED(tch, PLR_WANTED))
        strlcat(buf1, " ^R(WANTED)^N", sizeof(buf1));
      strlcat(buf1, "\r\n", sizeof(buf1));
      strlcat(buf, buf1, sizeof(buf));
      
      CGLOB = KNRM;
    }
  }
  
  if (num_can_see == 0)
    snprintf(buf2, sizeof(buf2), "%s\r\nNo-one at all!\r\n", buf);
  else if (num_can_see == 1)
    snprintf(buf2, sizeof(buf2), "%s\r\nOne lonely chummer displayed.\r\n", buf);
  else
    snprintf(buf2, sizeof(buf2), "%s\r\n%d chummers displayed.\r\n", buf, num_can_see);
  if (subcmd) {
    FILE *fl;
    static char buffer[MAX_STRING_LENGTH*4];
    char *temp = &buffer[0];
    const char *color;
    char *str = str_dup(buf2);
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
    fprintf(fl, "<HTML><BODY bgcolor=#11191C><PRE>%s</PRE></BODY></HTML>\r\n", &buffer[0]);
    fclose(fl);
  } else page_string(ch->desc, buf2, 1);
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
  
  strlcpy(buf, argument, sizeof(buf));
  while (*buf) {
    half_chop(buf, arg, buf1);
    if (*arg == '-') {
      mode = *(arg + 1);  /* just in case; we destroy arg in the switch */
      switch (mode) {
        case 'o':
        case 'k':
          outlaws = 1;
          playing = 1;
          strlcpy(buf, buf1, sizeof(buf));
          break;
        case 'p':
          playing = 1;
          strlcpy(buf, buf1, sizeof(buf));
          break;
        case 'd':
          deadweight = 1;
          strlcpy(buf, buf1, sizeof(buf));
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
  strlcpy(line, "Num  Name           State           Idle Login@   Site\r\n", sizeof(line));
  strlcat(line, "---- -------------- --------------- ---- -------- ---------------------------\r\n", sizeof(line));
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
      strlcpy(state, "Switched", sizeof(state));
    else
      strlcpy(state, connected_types[d->connected], sizeof(state));
    
    if (d->character && !d->connected)
      snprintf(idletime, sizeof(idletime), "%4d", d->character->char_specials.timer);
    else
      strlcpy(idletime, "", sizeof(idletime));
    
    format = "%-4d %-14s %-15s %-4s %-8s ";
    
    if (d->character && GET_CHAR_NAME(d->character)) {
      if (d->original)
        snprintf(line, sizeof(line), format, d->desc_num, GET_CHAR_NAME(d->original),
                state, idletime, timeptr);
      else
        snprintf(line, sizeof(line), format, d->desc_num, GET_CHAR_NAME(d->character),
                state, idletime, timeptr);
    } else
      snprintf(line, sizeof(line), format, d->desc_num, "UNDEFINED",
              state, idletime, timeptr);
    
    if (*d->host && GET_DESC_LEVEL(d) <= GET_LEVEL(ch))
      snprintf(ENDOF(line), sizeof(line) - strlen(line), "[%s]\r\n", d->host);
    else
      snprintf(line, sizeof(line), "[Hostname unknown]\r\n");
    
    if (d->connected) {
      snprintf(line2, sizeof(line2), "^g%s^n", line);
      strlcpy(line, line2, sizeof(line));
    }
    if ((d->connected && !d->character) || CAN_SEE(ch, d->character)) {
      send_to_char(line, ch);
      num_can_see++;
    }
  }
  
  send_to_char(ch, "\r\n%d visible sockets connected.\r\n", num_can_see);
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
      send_to_char(immlist, ch);
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
      send_to_char(ch, "%s\r\n", *awakemud_version);
      break;
    case SCMD_WHOAMI:
      send_to_char(ch, "%s\r\n", GET_CHAR_NAME(ch)) ;
      break;
    default:
      return;
  }
}

extern void nonsensical_reply(struct char_data *ch, const char *arg, const char *mode);

void perform_mortal_where(struct char_data * ch, char *arg)
{
  strlcpy(buf, "Players in socialization rooms\r\n-------\r\n", sizeof(buf));
  bool found_someone = FALSE;
  for (struct descriptor_data *d = descriptor_list; d; d = d->next) {
    if (!d->connected) {
      struct char_data *i = (d->original ? d->original : d->character);
      if (i && i->in_room && ROOM_FLAGGED(i->in_room, ROOM_ENCOURAGE_CONGREGATION) && CAN_SEE(ch, i)) {
        snprintf(ENDOF(buf), sizeof(buf) - strlen(buf), "%-20s - %s^n\r\n",
                GET_CHAR_NAME(i),
                GET_ROOM_NAME(i->in_room));
        found_someone = TRUE;
      }
    }
  }
  if (!found_someone) {
    strlcat(buf, "Nobody :(\r\n", sizeof(buf));
  }
  page_string(ch->desc, buf, 1);
}

void print_object_location(int num, struct obj_data *obj, struct char_data *ch,
                           int recur)
{
  if (strlen(buf) >= 7500)
    return;
  if (num > 0)
    snprintf(ENDOF(buf), sizeof(buf) - strlen(buf), "O%3d. %-25s - ", num,
            GET_OBJ_NAME(obj));
  else
    snprintf(ENDOF(buf), sizeof(buf) - strlen(buf), "%33s", " - ");
  
  if (obj->in_room)
    snprintf(ENDOF(buf), sizeof(buf) - strlen(buf), "[%5ld] %s\r\n", GET_ROOM_VNUM(obj->in_room), GET_ROOM_NAME(obj->in_room));
  else if (obj->carried_by) {
    snprintf(ENDOF(buf), sizeof(buf) - strlen(buf), "carried by %s", GET_CHAR_NAME(obj->carried_by));
    if (obj->carried_by->in_room) {
      snprintf(ENDOF(buf), sizeof(buf) - strlen(buf), " @ room %ld (%s)\r\n", GET_ROOM_VNUM(obj->carried_by->in_room), GET_ROOM_NAME(obj->carried_by->in_room));
    } else if (obj->carried_by->in_veh) {
      snprintf(ENDOF(buf), sizeof(buf) - strlen(buf), " @ veh %ld (%s)\r\n", GET_VEH_VNUM(obj->carried_by->in_veh), GET_VEH_NAME(obj->carried_by->in_veh));
    } else {
      snprintf(ENDOF(buf), sizeof(buf) - strlen(buf), " ^Rnowhere^n\r\n");
    }
  }
  else if (obj->worn_by) {
    snprintf(ENDOF(buf), sizeof(buf) - strlen(buf), "worn by %s", GET_CHAR_NAME(obj->worn_by));
    if (obj->worn_by->in_room) {
      snprintf(ENDOF(buf), sizeof(buf) - strlen(buf), " @ room %ld (%s)\r\n", GET_ROOM_VNUM(obj->worn_by->in_room), GET_ROOM_NAME(obj->worn_by->in_room));
    } else if (obj->worn_by->in_veh) {
      snprintf(ENDOF(buf), sizeof(buf) - strlen(buf), " @ veh %ld (%s)\r\n", GET_VEH_VNUM(obj->worn_by->in_veh), GET_VEH_NAME(obj->worn_by->in_veh));
    } else {
      snprintf(ENDOF(buf), sizeof(buf) - strlen(buf), " ^Rnowhere^n\r\n");
    }
  } else if (obj->in_obj) {
    snprintf(ENDOF(buf), sizeof(buf) - strlen(buf), "inside %s%s\r\n",
            GET_OBJ_NAME(obj->in_obj), (recur ? ", which is" : " "));
    if (recur)
      print_object_location(0, obj->in_obj, ch, recur);
  } else if (obj->in_veh) {
    snprintf(ENDOF(buf), sizeof(buf) - strlen(buf), "in %s @ %ld%s", GET_VEH_NAME(obj->in_veh), GET_ROOM_VNUM(get_obj_in_room(obj)), obj->in_veh->in_veh ? " (nested veh)" : "");
  } else
    snprintf(ENDOF(buf), sizeof(buf) - strlen(buf), "in an unknown location\r\n");
}

void perform_immort_where(struct char_data * ch, char *arg)
{
  struct char_data *i;
  struct descriptor_data *d;
  int num = 0, found = 0;
  int found2 = FALSE;
  
  if (!*arg)
  {
    strlcpy(buf, "Players\r\n-------\r\n", sizeof(buf));
    for (d = descriptor_list; d; d = d->next)
      if (!d->connected) {
        i = (d->original ? d->original : d->character);
        if (i && CAN_SEE(ch, i) && (i->in_room || i->in_veh)) {
          if (d->original)
            if (d->character->in_veh)
              snprintf(ENDOF(buf), sizeof(buf) - strlen(buf), "%-20s - [%6ld] %s^n (switched as %s) (in %s)\r\n",
                      GET_CHAR_NAME(i),
                      GET_ROOM_VNUM(get_ch_in_room(d->character)),
                      GET_ROOM_NAME(get_ch_in_room(d->character)),
                      GET_NAME(d->character),
                      GET_VEH_NAME(d->character->in_veh));
            else
              snprintf(ENDOF(buf), sizeof(buf) - strlen(buf), "%-20s - [%6ld] %s^n (in %s)\r\n",
                      GET_CHAR_NAME(i),
                      GET_ROOM_VNUM(get_ch_in_room(d->character)),
                      GET_ROOM_NAME(get_ch_in_room(d->character)),
                      GET_VEH_NAME(d->character->in_veh));
          
            else
              if (i->in_veh)
                snprintf(ENDOF(buf), sizeof(buf) - strlen(buf), "%-20s - [%6ld] %s^n (in %s)\r\n",
                        GET_CHAR_NAME(i),
                        GET_ROOM_VNUM(get_ch_in_room(i)),
                        GET_ROOM_NAME(get_ch_in_room(i)),
                        GET_VEH_NAME(i->in_veh));
              else
                snprintf(ENDOF(buf), sizeof(buf) - strlen(buf), "%-20s - [%6ld] %s^n\r\n",
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
        snprintf(ENDOF(buf), sizeof(buf) - strlen(buf), "M%3d. %-25s - [%5ld] %s^n", ++num,
                GET_NAME(i),
                GET_ROOM_VNUM(get_ch_in_room(i)),
                GET_ROOM_NAME(get_ch_in_room(i)));
        if (i->in_veh) {
          snprintf(ENDOF(buf), sizeof(buf) - strlen(buf), " (in %s^n)\r\n", GET_VEH_NAME(i->in_veh));
        } else {
          strlcat(buf, "\r\n", sizeof(buf));
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
    if (!can_hurt(ch, victim, 0, TRUE)) {
      send_to_char("This NPC has been made unkillable by staff.\r\n", ch);
      return;
    }
    
    // Pick out the victim's cyberware, if any. TODO: Player cyberware.
    bool use_cyber_implants = FALSE;
    int unarmed_dangerliciousness_boost = 0;
    for (struct obj_data *obj = victim->cyberware; obj; obj = obj->next_content) {
      if (!GET_CYBERWARE_IS_DISABLED(obj)) {
        switch (GET_CYBERWARE_TYPE(obj)) {
          case CYB_CLIMBINGCLAWS:
          case CYB_FIN:
          case CYB_HANDBLADE:
          case CYB_HANDRAZOR:
          case CYB_HANDSPUR:
          case CYB_FOOTANCHOR:
            use_cyber_implants = TRUE;
            break;
        }
      } else if (GET_CYBERWARE_TYPE(obj) == CYB_BONELACING) {
        switch (GET_CYBERWARE_LACING_TYPE(obj)) {
          case BONE_PLASTIC:
            unarmed_dangerliciousness_boost = MAX(unarmed_dangerliciousness_boost, 2);
            break;
          case BONE_ALUMINIUM:
          case BONE_CERAMIC:
            unarmed_dangerliciousness_boost = MAX(unarmed_dangerliciousness_boost, 3);
            break;
          case BONE_TITANIUM:
            unarmed_dangerliciousness_boost = MAX(unarmed_dangerliciousness_boost, 4);
            break;
        }
      }
    }
    
    // Armor comparisons.
    diff = (GET_BALLISTIC(victim) - GET_BALLISTIC(ch));
    diff += (GET_IMPACT(victim) - GET_IMPACT(ch));
    
    // Stat comparisons.
    diff += (GET_BOD(victim) - GET_BOD(ch));
    diff += (GET_QUI(victim) - GET_QUI(ch));
    diff += (GET_STR(victim) - GET_STR(ch));
    diff += (GET_REA(victim) - GET_REA(ch));
    diff += (GET_INIT_DICE(victim) - GET_INIT_DICE(ch));
    
    if (GET_MAG(victim) >= 100) {
      diff += (int)((GET_MAG(victim) - GET_MAG(ch)) / 100);
      diff += GET_SKILL(victim, SKILL_SORCERY);
    }
    
    // Pool comparisons.
    diff += (GET_COMBAT(victim) - GET_COMBAT(ch));
    
    // Skill comparisons.
    if (GET_MAG(ch) >= 100 && (IS_NPC(ch) || (GET_TRADITION(ch) == TRAD_HERMETIC ||
                                              GET_TRADITION(ch) == TRAD_SHAMANIC)))
      diff -= GET_SKILL(ch, SKILL_SORCERY);
    
    if (GET_EQ(victim, WEAR_WIELD))
      diff += GET_SKILL(victim, GET_OBJ_VAL(GET_EQ(victim, WEAR_WIELD), 4));
    else if (use_cyber_implants)
      diff += GET_SKILL(victim, SKILL_CYBER_IMPLANTS);
    else
      diff += GET_SKILL(victim, SKILL_UNARMED_COMBAT) + unarmed_dangerliciousness_boost;
      
    if (GET_EQ(ch, WEAR_WIELD))
      diff -= GET_SKILL(ch, GET_OBJ_VAL(GET_EQ(ch, WEAR_WIELD), 4));
    else
      diff -= GET_SKILL(ch, SKILL_UNARMED_COMBAT);
    
    if (diff <= -25)
      send_to_char("Now where did that chicken go?\r\n", ch);
    else if (diff <= -15)
      send_to_char("You could kill it with a needle!\r\n", ch);
    else if (diff <= -8)
      send_to_char("Easy.\r\n", ch);
    else if (diff <= -4)
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
      send_to_char(ch, "You don't see anyone named '%s' here.\r\n", buf);
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
    snprintf(buf, sizeof(buf), "The following %s are available to you:\r\n",
            socials ? "socials" : "commands");
    mode_all = TRUE;
  } else {
    snprintf(buf, sizeof(buf), "The following %s beginning with '%s' are available to you:\r\n",
            socials ? "socials" : "commands", arg);
    mode_all = FALSE;
  }
  
  if (PLR_FLAGGED(ch, PLR_MATRIX)) {
    for (no = 1, cmd_num = 1; *mtx_info[cmd_num].command != '\n';cmd_num++) {
      // Skip any commands that don't match the prefix provided.
      if (!mode_all && *arg && !is_abbrev(arg, mtx_info[cmd_num].command))
        continue;
      if (mtx_info[cmd_num].minimum_level >= 0 &&
          ((!IS_NPC(vict) && GET_REAL_LEVEL(vict) >= mtx_info[cmd_num].minimum_level) ||
           (IS_NPC(vict) && vict->desc->original && GET_REAL_LEVEL(vict->desc->original) >= mtx_info[cmd_num].minimum_level))) {
            snprintf(ENDOF(buf), sizeof(buf) - strlen(buf), "%-13s", mtx_info[cmd_num].command);
            if (!(no % 7) || PRF_FLAGGED(ch, PRF_SCREENREADER))
              strlcat(buf, "\r\n", sizeof(buf));
            no++;
            if (*mtx_info[cmd_num].command == '\n')
              break;
          }
    }
  } else if (PLR_FLAGGED(ch, PLR_REMOTE) || AFF_FLAGGED(ch, AFF_RIG)) {
    for (no = 1, cmd_num = 1; *rig_info[cmd_num].command != '\n';cmd_num++) {
      // Skip any commands that don't match the prefix provided.
      if (!mode_all && *arg && !is_abbrev(arg, rig_info[cmd_num].command))
        continue;
      
      if (rig_info[cmd_num].minimum_level >= 0 &&
          ((!IS_NPC(vict) && GET_REAL_LEVEL(vict) >= rig_info[cmd_num].minimum_level) ||
           (IS_NPC(vict) && vict->desc->original && GET_REAL_LEVEL(vict->desc->original) >= rig_info[cmd_num].minimum_level))) {
            snprintf(ENDOF(buf), sizeof(buf) - strlen(buf), "%-13s", rig_info[cmd_num].command);
            if (!(no % 7) || PRF_FLAGGED(ch, PRF_SCREENREADER))
              strlcat(buf, "\r\n", sizeof(buf));
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
        snprintf(ENDOF(buf), sizeof(buf) - strlen(buf), "%-13s", cmd_info[i].command);
        if (!(no % 7) || PRF_FLAGGED(ch, PRF_SCREENREADER))
          strlcat(buf, "\r\n", sizeof(buf));
        no++;
      }
    }
  }
  strlcat(buf, "\r\n", sizeof(buf));
  send_to_char(buf, ch);
}

// TODO: rscan, which is like scan but just shows room names

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
    struct room_data *in_room = get_ch_in_room(ch);
    for (i = 0; i < NUM_OF_DIRS; ++i) {
      if (CAN_GO(ch, i)) {
        if (EXIT(ch, i)->to_room == in_room) {
          send_to_char(ch, "%s: More of the same.\r\n", dirs[i]);
          continue;
        }
        
        onethere = FALSE;
        if (!((!infra && light_level(EXIT(ch, i)->to_room) == LIGHT_FULLDARK) ||
              ((!infra || !lowlight) && (light_level(EXIT(ch, i)->to_room) == LIGHT_MINLIGHT || light_level(EXIT(ch, i)->to_room) == LIGHT_PARTLIGHT)))) {
          strlcpy(buf1, "", sizeof(buf1));
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
              snprintf(ENDOF(buf1), sizeof(buf1) - strlen(buf1), "  %s\r\n", GET_NAME(list));
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
            snprintf(ENDOF(buf1), sizeof(buf1) - strlen(buf1), "  %s (%s)\r\n", GET_VEH_NAME(veh), (get_speed(veh) ? "Moving" : "Stationary"));
            onethere = TRUE;
            anythere = TRUE;
          }
        }
        if (onethere) {
          snprintf(buf2, sizeof(buf2), "%s %s:\r\n%s\r\n", dirs[i], dist_name[0], buf1);
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
          strlcpy(buf1, "", sizeof(buf1));
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
              snprintf(ENDOF(buf1), sizeof(buf1) - strlen(buf1), "  %s\r\n", GET_NAME(list));
              onethere = TRUE;
              anythere = TRUE;
            }
          
          ch->in_room = EXIT(ch, i)->to_room;
          
          if (onethere) {
            snprintf(buf2, sizeof(buf2), "%s %s:\r\n%s\r\n", dirs[i], dist_name[j], buf1);
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
  send_to_char(ch, "Position set. You will appear as '(your character) %s^n'\r\n", GET_DEFPOS(ch));
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
  switch (get_armor_penalty_grade(targ)) {
    case ARMOR_PENALTY_TOTAL:
      send_to_char("  Bulky Armor (Insane)\r\n", ch);
      break;
    case ARMOR_PENALTY_HEAVY:
      send_to_char("  Bulky Armor (Serious)\r\n", ch);
      break;
    case ARMOR_PENALTY_MEDIUM:
      send_to_char("  Bulky Armor (Moderate)\r\n", ch);
      break;
    case ARMOR_PENALTY_LIGHT:
      send_to_char("  Bulky Armor (Light)\r\n", ch);
      break;
  }
  if (GET_REACH(targ))
    send_to_char(ch, "Extra Reach (%dm)\r\n", GET_REACH(targ));
  if (GET_DRUG_AFFECT(targ) && GET_DRUG_STAGE(targ) > 0)
    send_to_char(ch, "  %s (%s)\r\n", drug_types[GET_DRUG_AFFECT(targ)].name, GET_DRUG_STAGE(targ) == 1 ? "Up" : "Down");
  for (struct sustain_data *sust = GET_SUSTAINED(targ); sust; sust = sust->next)
    if (!sust->caster) {
      strlcpy(buf, spells[sust->spell].name, sizeof(buf));
      if (sust->spell == SPELL_INCATTR || sust->spell == SPELL_INCCYATTR ||
          sust->spell == SPELL_DECATTR || sust->spell == SPELL_DECCYATTR)
        snprintf(ENDOF(buf), sizeof(buf) - strlen(buf), " (%s)", attributes[sust->subtype]);
      send_to_char(buf, ch);
      send_to_char("\r\n", ch);
    }
  if (GET_SUSTAINED_NUM(targ)) {
    send_to_char("You are sustaining:\r\n", ch);
    int i = 1;
    for (struct sustain_data *sust = GET_SUSTAINED(targ); sust; sust = sust->next)
      if (sust->caster || sust->spirit == targ) {
        strlcpy(buf, spells[sust->spell].name, sizeof(buf));
        if (sust->spell == SPELL_INCATTR || sust->spell == SPELL_INCCYATTR ||
            sust->spell == SPELL_DECATTR || sust->spell == SPELL_DECCYATTR)
          snprintf(ENDOF(buf), sizeof(buf) - strlen(buf), " (%s)", attributes[sust->subtype]);
        send_to_char(ch, "%d) %s (%d successes)", i, buf, sust->success);
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
#ifdef USE_QUEST_LOCATION_CODE
    if (quest_table[GET_QUEST(ch)].location)
      snprintf(buf, sizeof(buf), "At %s, %s told you: \r\n%s", quest_table[GET_QUEST(ch)].location, GET_NAME(mob_proto+real_mobile(quest_table[GET_QUEST(ch)].johnson)),
              quest_table[GET_QUEST(ch)].info);
    else
#endif
      snprintf(buf, sizeof(buf), "%s told you: \r\n%s", GET_NAME(mob_proto+real_mobile(quest_table[GET_QUEST(ch)].johnson)),
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
  send_to_char(ch, "Your current TKE is %d.\r\n", GET_TKE(ch));
}

#define LEADERBOARD_SYNTAX_STRING "Syntax: leaderboard <option>, where option is one of: tke, reputation, notoriety, nuyen, syspoints\r\n"
ACMD(do_leaderboard) {
  // leaderboard <tke|rep|notor|nuyen|sysp>
  skip_spaces(&argument);
  if (!*argument) {
    send_to_char(LEADERBOARD_SYNTAX_STRING, ch);
    return;
  }
  
  const char *display_string = NULL, *query_string = NULL;
  
  if (!strncmp(argument, "tke", strlen(argument))) {
    display_string = "TKE";
    query_string = "tke";
  }
  
  else if (!strncmp(argument, "reputation", strlen(argument))) {
    display_string = "reputation";
    query_string = "rep";
  }
  
  else if (!strncmp(argument, "notoriety", strlen(argument))) {
    display_string = "notoriety";
    query_string = "notor";
  }
  
  else if (!strncmp(argument, "nuyen", strlen(argument))) {
    display_string = "nuyen";
    query_string = "bank + cash";
  }
  
  else if (!strncmp(argument, "syspoints", strlen(argument))) {
    display_string = "syspoints";
    query_string = "syspoints";
  }
  
  else {
    send_to_char(LEADERBOARD_SYNTAX_STRING, ch);
    return;
  }
  
  MYSQL_RES *res;
  MYSQL_ROW row;
  
  // Sanitization not required here-- they're constant strings.
  snprintf(buf, sizeof(buf), "SELECT name, %s FROM pfiles "
               "  WHERE %s > 0 "
               "  AND rank = 1 "
               "  AND TKE > 0 "
               "  AND name != 'deleted' "
               "ORDER BY %s DESC LIMIT 10;", 
               query_string, query_string, query_string);
  
  mysql_wrapper(mysql, buf);
  if (!(res = mysql_use_result(mysql))) {
    snprintf(buf, sizeof(buf), "SYSERR: Failed to access %s leaderboard data!", display_string);
    mudlog(buf, ch, LOG_SYSLOG, TRUE);
    send_to_char(ch, "Sorry, the leaderboard system is offline at the moment.\r\n");
    return;
  }
  
  send_to_char(ch, "^CTop 10 characters by %s:^n\r\n", display_string);
  int counter = 1;
  while ((row = mysql_fetch_row(res))) {
    send_to_char(ch, "%2d) %-20s: %-15s\r\n", counter++, row[0], row[1]);
  }
  if (counter == 1)
    send_to_char(ch, "...Nobody! Looks like a great place to make your mark.\r\n");
  mysql_free_result(res);
}

ACMD(do_search) {
  bool found_something = FALSE;
  bool has_secrets = FALSE;
  
  act("$n searches the area.", TRUE, ch, 0, 0, TO_ROOM);
  
  for (int dir = 0; dir < NUM_OF_DIRS; dir++) {
    if (EXIT(ch, dir) && IS_SET(EXIT(ch, dir)->exit_info, EX_HIDDEN)) {
      has_secrets = TRUE;
      if (success_test(GET_INT(ch) + GET_POWER(ch, ADEPT_IMPROVED_PERCEPT), EXIT(ch, dir)->hidden) > 0) {
        if (!found_something)
          send_to_char("You begin searching the area for secrets...\r\n", ch);
        REMOVE_BIT(EXIT(ch, dir)->exit_info, EX_HIDDEN);
        send_to_char(ch, "You discover an exit to %s.\r\n", thedirs[dir]);
        found_something = TRUE;
      }
    }
  }
  
  if (!found_something) {
    send_to_char("You search the area for secrets, but fail to turn anything up.\r\n", ch);
    
    if (has_secrets && success_test(GET_INT(ch), 4))
      send_to_char("You feel like there's still something to uncover here...\r\n", ch);
  }
}
