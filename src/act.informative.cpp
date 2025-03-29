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
#include <unordered_map>
#include <vector>
#include <algorithm>

// using namespace std;

#if !defined(WIN32) || defined(__CYGWIN__)
#include <sys/time.h>
#endif

#include "structs.hpp"
#include "utils.hpp"
#include "comm.hpp"
#include "interpreter.hpp"
#include "handler.hpp"
#include "db.hpp"
#include "dblist.hpp"
#include "screen.hpp"
#include "list.hpp"
#include "awake.hpp"
#include "constants.hpp"
#include "quest.hpp"
#include "transport.hpp"
#include "newdb.hpp"
#include "bullet_pants.hpp"
#include "config.hpp"
#include "newmail.hpp"
#include "ignore_system.hpp"
#include "newmagic.hpp"
#include "newmatrix.hpp"
#include "lifestyles.hpp"
#include "newhouse.hpp"
#include "zoomies.hpp"
#include "vehicles.hpp"
#include "moderation.hpp"
#include "player_exdescs.hpp"
#include "matrix_storage.hpp"

const char *CCHAR;

extern bool DISPLAY_HELPFUL_STRINGS_FOR_MOB_FUNCS;

/* extern variables */
extern class objList ObjList;
extern class helpList Help;
extern class helpList WizHelp;
extern struct time_info_data time_info;

extern const char *spirit_powers_from_bit[];

extern char *short_object(int virt, int where);
extern const char *dist_name[];

extern int same_obj(struct obj_data * obj1, struct obj_data * obj2);
extern int find_sight(struct char_data *ch);
extern int belongs_to(struct char_data *ch, struct obj_data *obj);
extern int calculate_vehicle_entry_load(struct veh_data *veh);
extern unsigned int get_johnson_overall_max_rep(struct char_data *johnson);
extern const char *get_crap_count_string(int crap_count, const char *default_color = "^n", bool screenreader = FALSE);
extern void display_gamba_ledger_leaderboard(struct char_data *ch);
const char *convert_and_write_string_to_file(const char *str, const char *path);

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
extern SPECIAL(pocsec_unlocker);
extern SPECIAL(soulbound_unbinder);
extern SPECIAL(bank);
extern WSPEC(monowhip);

extern bool trainable_attribute_is_maximized(struct char_data *ch, int attribute);
extern float get_bulletpants_weight(struct char_data *ch);
extern bool can_hurt(struct char_data *ch, struct char_data *victim, int attacktype, bool include_func_protections);
extern const char *get_level_wholist_color(int level);
extern bool cyber_is_retractable(struct obj_data *cyber);
extern bool precipitation_is_snow(int jurisdiction);

bool ch_can_see_vict_in_where(struct char_data *ch, struct char_data *vict);
void write_gsgp_file(int player_count, const char *path);

#ifdef USE_PRIVATE_CE_WORLD
  extern void display_secret_info_about_object(struct char_data *ch, struct obj_data *obj);
#endif

extern struct teach_data teachers[];

extern const char *pc_readable_extra_bits[];

extern struct elevator_data *elevator;
extern int num_elevators;

const char *get_command_hints_for_obj(struct obj_data *obj);

// ACMD_DECLARE(do_unsupported_command);

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

void display_room_name(struct char_data *ch, struct room_data *in_room, bool in_veh);
void display_room_desc(struct char_data *ch);

/* end blood stuff */

char *make_desc(struct char_data *ch, struct char_data *i, char *buf, int act, bool dont_capitalize_a_an, size_t buf_size)
{
  char buf2[MAX_STRING_LENGTH];
  if (!IS_NPC(i)
      && !IS_SENATOR(ch)
      && ch != i
      && ((GET_EQ(i, WEAR_HEAD) && GET_WORN_CONCEAL_RATING(GET_EQ(i, WEAR_HEAD)) > 1)
          || (GET_EQ(i, WEAR_FACE) && GET_WORN_CONCEAL_RATING(GET_EQ(i, WEAR_FACE)) > 1))
      && (act == 2
          || success_test(GET_INT(ch) + GET_POWER(ch, ADEPT_IMPROVED_PERCEPT),
                          (GET_EQ(i, WEAR_HEAD) ? GET_WORN_CONCEAL_RATING(GET_EQ(i, WEAR_HEAD)) : 0) +
                          (GET_EQ(i, WEAR_FACE) ? GET_WORN_CONCEAL_RATING(GET_EQ(i, WEAR_FACE)) : 0)) < 1))
  {
    int conceal = (GET_EQ(i, WEAR_ABOUT) ? GET_WORN_CONCEAL_RATING(GET_EQ(i, WEAR_ABOUT)) : 0) +
    (GET_EQ(i, WEAR_BODY) ? GET_WORN_CONCEAL_RATING(GET_EQ(i, WEAR_BODY)) : 0) +
    (GET_EQ(i, WEAR_UNDER) ? GET_WORN_CONCEAL_RATING(GET_EQ(i, WEAR_UNDER)) : 0);
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

    // Gender seeming.
    if (perception_successes > 2)
      snprintf(ENDOF(buf), buf_size - strlen(buf), " %s", genders[(int)GET_PRONOUNS(i)]);

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
      else if ((mem = safe_found_mem(ch, i)))
        snprintf(ENDOF(buf), buf_size - strlen(buf), " (%s)", CAP(mem->mem));
    } else if ((mem = safe_found_mem(ch, i)) && act != 2)
      strlcpy(buf, CAP(mem->mem), buf_size);
    else
      strlcpy(buf, dont_capitalize_a_an ? decapitalize_a_an(CAP(GET_NAME(i))) : CAP(GET_NAME(i)), buf_size);
  }
  if (GET_SUSTAINED(i) && SEES_ASTRAL(ch))
  {
    bool has_aura = FALSE;
    for (struct sustain_data *sust = GET_SUSTAINED(i); sust; sust = sust->next) {
      if (!sust->caster) {
        strlcat(buf, ", surrounded by a spell aura", buf_size);
        has_aura = TRUE;
        break;
      }
    }

    if (MOB_FLAGGED(i, MOB_FLAMEAURA) || affected_by_spell(i, SPELL_FLAME_AURA)) {
      if (has_aura)
        strlcat(buf, " and flames", buf_size);
      else
        strlcat(buf, ", surrounded by flames", buf_size);
    }
  } else if (MOB_FLAGGED(i, MOB_FLAMEAURA) || affected_by_spell(i, SPELL_FLAME_AURA)) {
    strlcat(buf, ", surrounded by flames", buf_size);
  }

  if (!IS_NPC(i) && PRF_FLAGGED(ch, PRF_LONGWEAPON) && GET_EQ(i, WEAR_WIELD))
    snprintf(ENDOF(buf), buf_size - strlen(buf), ", wielding %s", decapitalize_a_an(GET_OBJ_NAME(GET_EQ(i, WEAR_WIELD))));

  if (AFF_FLAGGED(i, AFF_MANIFEST) && !SEES_ASTRAL(ch))
  {
    snprintf(buf2, sizeof(buf2), "The ghostly image of %s", decapitalize_a_an(buf));
    strlcpy(buf, buf2, buf_size);
  }
  return buf;
}

void get_obj_condition(struct char_data *ch, struct obj_data *obj)
{
  if (GET_OBJ_EXTRA(obj).IsSet(ITEM_EXTRA_CORPSE))
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
  SPECIAL(floor_usable_radio);
  SPECIAL(pocket_sec);

  *buf = '\0';

  if (GET_OBJ_TYPE(object) == ITEM_CREATIVE_EFFORT) {
    strlcat(buf, "^c(Art) ^n", sizeof(buf));
  }
  
  if ((mode == SHOW_MODE_ON_GROUND) && object->text.room_desc) {
    strlcat(buf, CCHAR ? CCHAR : "", sizeof(buf));

    if (object->graffiti) {
      if (GET_OBJ_VNUM(object) == OBJ_CUSTOM_ART)
        strlcat(buf, "^g", sizeof(buf));
      strlcat(buf, object->graffiti, sizeof(buf));
      strlcat(buf, "^n", sizeof(buf));
    }
    else {
      // Gun magazines get special consideration.
      if (GET_OBJ_TYPE(object) == ITEM_GUN_MAGAZINE && GET_MAGAZINE_BONDED_MAXAMMO(object)) {
        snprintf(buf, sizeof(buf), "^gA%s %d-round %s %s magazine has been left here.^n",
          GET_MAGAZINE_AMMO_COUNT(object) <= 0 ? "n empty" : "",
          GET_MAGAZINE_BONDED_MAXAMMO(object),
          ammo_type[GET_MAGAZINE_AMMO_TYPE(object)].name,
          weapon_types[GET_MAGAZINE_BONDED_ATTACKTYPE(object)]
        );
      // As do ammo boxes.
      } else if (GET_OBJ_TYPE(object) == ITEM_GUN_AMMO) {
        snprintf(buf, sizeof(buf), "^gA metal box of %s has been left here.^n", get_ammo_representation(GET_AMMOBOX_WEAPON(object), GET_AMMOBOX_TYPE(object), 0, ch));
      } else {
        if (GET_OBJ_TYPE(object) == ITEM_WORKSHOP && GET_WORKSHOP_GRADE(object) == TYPE_WORKSHOP) {
          if (GET_WORKSHOP_IS_SETUP(object) && !GET_WORKSHOP_UNPACK_TICKS(object))
            strlcat(buf, "^n(Deployed) ^g", sizeof(buf));
          else if (GET_WORKSHOP_UNPACK_TICKS(object))
            strlcat(buf, "^n(Half-Packed) ^g", sizeof(buf));
        }
        char replaced_colors[strlen(object->text.room_desc) * 2];
        replace_substring(object->text.room_desc, buf2, "^n", "^g");
        replace_substring(buf2, replaced_colors, "^N", "^g");
        strlcat(buf, replaced_colors, sizeof(buf));
      }
    }
  }
  else if (GET_OBJ_NAME(object) && (mode == SHOW_MODE_IN_INVENTORY || mode == SHOW_MODE_INSIDE_CONTAINER)) {
    strlcat(buf, GET_OBJ_NAME(object), sizeof(buf));
    if (GET_OBJ_TYPE(object) == ITEM_DESIGN)
      strlcat(buf, " (Plan)", sizeof(buf));
    if (GET_OBJ_VNUM(object) == 108 && !GET_OBJ_TIMER(object))
      strlcat(buf, " (Uncooked)", sizeof(buf));
    if (GET_OBJ_TYPE(object) == ITEM_FOCUS && GET_FOCUS_GEAS(object) == GET_IDNUM(ch))
      strlcat(buf, " ^Y(Geas)^n", sizeof(buf));
    if (GET_OBJ_TYPE(object) == ITEM_WEAPON && WEAPON_IS_FOCUS(object) && GET_WEAPON_FOCUS_GEAS(object) == GET_IDNUM(ch))
      strlcat(buf, " ^Y(Geas)^n", sizeof(buf));
    if (GET_OBJ_TYPE(object) == ITEM_GUN_AMMO
        || (GET_OBJ_TYPE(object) == ITEM_DECK_ACCESSORY && GET_DECK_ACCESSORY_TYPE(object) == TYPE_PARTS))
      {
        strlcat(buf, " (Combinable)", sizeof(buf));
      }
    if (GET_OBJ_SPEC(object) == pocket_sec && amount_of_mail_waiting(ch) > 0) {
      strlcat(buf, " ^y(Mail Waiting)^n", sizeof(buf));
    }
    if (IS_OBJ_STAT(object, ITEM_EXTRA_KEPT)) {
      strlcat(buf, " ^c(kept)^n", sizeof(buf));
    }

    if (IS_OBJ_STAT(object, ITEM_EXTRA_HARDENED_ARMOR)) {
      if (GET_WORN_HARDENED_ARMOR_CUSTOMIZED_FOR(object) == GET_IDNUM(ch)) {
        strlcat(buf, " (yours)", sizeof(buf));
      } else if (GET_WORN_HARDENED_ARMOR_CUSTOMIZED_FOR(object) == -1) {
        strlcat(buf, " ^g(not yet customized)", sizeof(buf));
      } else if (object->carried_by && object->carried_by != ch && IS_SENATOR(ch)) {
        if (GET_WORN_HARDENED_ARMOR_CUSTOMIZED_FOR(object) == GET_IDNUM(object->carried_by))
          strlcat(buf, " (theirs)", sizeof(buf));
        else if (GET_WORN_HARDENED_ARMOR_CUSTOMIZED_FOR(object) == 0)
          strlcat(buf, " ^y(npc's)", sizeof(buf));
        else
          strlcat(buf, " ^y(other pc's)", sizeof(buf));
      } else {
        strlcat(buf, " ^y(unusable)", sizeof(buf));
      }
    } else if (blocked_by_soulbinding(ch, object, FALSE)) {
      snprintf(ENDOF(buf), sizeof(buf) - strlen(buf), " ^r(soulbound to %s)", get_soulbound_name(object));
    } else if (get_soulbound_idnum(object) == GET_IDNUM(ch)) {
      strlcat(buf, " ^L(soulbound)^n", sizeof(buf));
    } else if (get_soulbound_idnum(object) != SB_CODE_OBJ_CANT_BE_SOULBOUND) {
      strlcat(buf, " ^L(not yet bound)^n", sizeof(buf));
    }

    if (GET_OBJ_VNUM(object) == OBJ_SHOPCONTAINER && object->contains) {
      if (blocked_by_soulbinding(ch, object->contains, FALSE)) {
        snprintf(ENDOF(buf), sizeof(buf) - strlen(buf), " ^r(soulbound to %s)", get_soulbound_name(object->contains));
      } else if (get_soulbound_idnum(object->contains) == GET_IDNUM(ch)) {
        strlcat(buf, " ^L(soulbound)^n", sizeof(buf));
      } else if (get_soulbound_idnum(object) == SB_CODE_OBJ_CANT_BE_SOULBOUND) {
        strlcat(buf, " ^L(not yet bound)^n", sizeof(buf));
      }
    }
  }
  else if (GET_OBJ_NAME(object) && ((mode == 3) || (mode == 4) || (mode == SHOW_MODE_OWN_EQUIPMENT) || (mode == SHOW_MODE_SOMEONE_ELSES_EQUIPMENT))) {
    strlcat(buf, GET_OBJ_NAME(object), sizeof(buf));

    if (IS_OBJ_STAT(object, ITEM_EXTRA_CONCEALED_IN_EQ) && object->worn_by == ch)
      strlcat(buf, " ^L(concealed from other players)^n", sizeof(buf));
  }
  else if (mode == SHOW_MODE_JUST_DESCRIPTION) {
    // Deliberately using strlcpy here to overwrite the (Art) tag.
    if (GET_OBJ_DESC(object))
      strlcpy(buf, GET_OBJ_DESC(object), sizeof(buf));
    else
      strlcpy(buf, "You see nothing special..", sizeof(buf));
  }
  else if (mode == SHOW_MODE_CONTAINED_OBJ) {
    snprintf(ENDOF(buf), sizeof(buf) - strlen(buf), "\t\t\t\t%s^n", GET_OBJ_NAME(object));
  }

  if (mode == SHOW_MODE_SOMEONE_ELSES_EQUIPMENT) {
    if (GET_OBJ_TYPE(object) == ITEM_HOLSTER) {
      if (object->contains) {
        snprintf(ENDOF(buf), sizeof(buf) - strlen(buf), " (Holding %s^n)", GET_OBJ_NAME(object->contains));
      }
    }
  }
  else if (mode == SHOW_MODE_OWN_EQUIPMENT || mode == SHOW_MODE_CONTAINED_OBJ) {
    if (GET_OBJ_TYPE(object) == ITEM_HOLSTER) {
      if (object->contains)
        snprintf(ENDOF(buf), sizeof(buf) - strlen(buf), " (Holding %s^n)", GET_OBJ_NAME(object->contains));
      if (GET_HOLSTER_READY_STATUS(object) == 1 && get_obj_possessor(object) == ch)
        strlcat(buf, " ^Y(Ready)", sizeof(buf));
    }
    else if (GET_OBJ_TYPE(object) == ITEM_WORN && object->contains && !PRF_FLAGGED(ch, PRF_COMPACT)) {
      strlcat(buf, " carrying:", sizeof(buf));
    }
    else if (GET_OBJ_TYPE(object) == ITEM_FOCUS) {
      if (GET_FOCUS_ACTIVATED(object))
        strlcat(buf, " ^m(Activated Focus)^n", sizeof(buf));
      if (GET_FOCUS_BONDED_TO(object) == GET_IDNUM(ch) && GET_FOCUS_GEAS(object) == GET_IDNUM(ch))
        strlcat(buf, " ^Y(Geas)^n", sizeof(buf));
    }
    else if (GET_OBJ_TYPE(object) == ITEM_WEAPON && WEAPON_IS_FOCUS(object) && is_weapon_focus_usable_by(object, ch)) {
      if (GET_WEAPON_FOCUS_GEAS(object) == GET_IDNUM(ch))
        strlcat(buf, " ^Y(Geas)^n", sizeof(buf));
      
      struct obj_data *wielded = NULL;
      if ((wielded = GET_EQ(ch, WEAR_WIELD)) && wielded->item_number == object->item_number)
        strlcat(buf, " ^m(Activated Focus)^n", sizeof(buf));
      if ((wielded = GET_EQ(ch, WEAR_HOLD)) && wielded->item_number == object->item_number)
        strlcat(buf, " ^m(Activated Focus)^n", sizeof(buf));
    }

    if (IS_OBJ_STAT(object, ITEM_EXTRA_KEPT)) {
      strlcat(buf, " ^c(kept)^n", sizeof(buf));
    }
  }

  if (mode != 3) {
    if (OBJ_IS_FULLY_DAMAGED(object)) {
      strlcat(buf, " ^R(almost destroyed)^n", sizeof(buf));
    } else if (GET_OBJ_CONDITION(object) * 100 / MAX(1, GET_OBJ_BARRIER(object)) < 100)
      strlcat(buf, " ^r(damaged)^n", sizeof(buf));

    if ((GET_OBJ_TYPE(object) == ITEM_FOCUS || IS_OBJ_STAT(object, ITEM_EXTRA_MAGIC))
        && SEES_ASTRAL(ch)) {
      strlcat(buf, " ^Y(magic aura)", sizeof(buf));
    }

    if (IS_OBJ_STAT(object, ITEM_EXTRA_GLOW)) {
      strlcat(buf, " ^W(glowing)", sizeof(buf));
    }

    if (IS_OBJ_STAT(object, ITEM_EXTRA_HUM)) {
      strlcat(buf, " ^c(humming)", sizeof(buf));
    }

    if (GET_OBJ_QUEST_CHAR_ID(object) && !IS_OBJ_STAT(object, ITEM_EXTRA_CORPSE)) {
      if (GET_OBJ_QUEST_CHAR_ID(object) == GET_IDNUM_EVEN_IF_PROJECTING(ch))
        strlcat(buf, " ^Y(Quest)^n", sizeof(buf));
      else if (!ch_is_blocked_by_quest_protections(ch, object, FALSE, TRUE))
        strlcat(buf, " ^Y(Group Quest)^n", sizeof(buf));
      else
        strlcat(buf, " ^m(Protected)^n", sizeof(buf));
    }
  }

  // Special case: Radio is spec-flagged and should show a help message.
  // This is pretty much only true for the Docwagon radios.
  if (mode == SHOW_MODE_ON_GROUND && object->text.room_desc) {
    if (GET_OBJ_SPEC(object) == floor_usable_radio) {
      strlcat(buf, "\r\n^y...It's free to use. See ^YHELP RADIO^y for more.^n", sizeof(buf));
    }
    else if (GET_OBJ_SPEC(object) == bank) {
      strlcat(buf, "\r\n^y...See ^YHELP ATM^y for details.^n", sizeof(buf));
    }
  }

  strlcat(buf, "^N\r\n", sizeof(buf));
  send_to_char(buf, ch);
  if ((mode == SHOW_MODE_OWN_EQUIPMENT || mode == SHOW_MODE_CONTAINED_OBJ) && !PRF_FLAGGED(ch, PRF_COMPACT))
    if (GET_OBJ_TYPE(object) == ITEM_WORN && object->contains)
    {
      for (struct obj_data *cont = object->contains; cont; cont = cont->next_content)
        show_obj_to_char(cont, ch, SHOW_MODE_CONTAINED_OBJ);
    }

}

void show_veh_to_char(struct veh_data * vehicle, struct char_data * ch)
{
  *buf = '\0';

  strlcpy(buf, CCHAR ? CCHAR : "", sizeof(buf));

  snprintf(buf2, sizeof(buf2), "%s", replace_neutral_color_codes(GET_VEH_NAME_NOFORMAT(vehicle), "^y"));
  const char *veh_name = buf2;

  if (vehicle->damage >= VEH_DAM_THRESHOLD_DESTROYED)
  {
    snprintf(ENDOF(buf), sizeof(buf) - strlen(buf), "%s^y lies here wrecked.", CAP(veh_name));
  }

  else {
    bool should_capitalize = TRUE;
    if (vehicle->type == VEH_BIKE && vehicle->people && CAN_SEE(ch, vehicle->people)) {
      should_capitalize = FALSE;
      snprintf(ENDOF(buf), sizeof(buf) - strlen(buf), "%s^y sitting on ", CAP(safe_found_mem(ch, vehicle->people) ?
                                                safe_found_mem(ch, vehicle->people)->mem :
                                                GET_NAME(vehicle->people)));
    }

    if (GET_VEH_DEFPOS(vehicle) && (vehicle->cspeed == SPEED_OFF || vehicle->cspeed == SPEED_IDLE)) {
      if (vehicle->towing) {
        bool towing_comma = ispunct(get_final_character_from_string(get_string_after_color_code_removal(GET_VEH_DEFPOS(vehicle), NULL)));
        snprintf(ENDOF(buf), sizeof(buf) - strlen(buf), "%s^y %s^y%stowing %s^y%s", 
                 should_capitalize ? CAP(veh_name) : decapitalize_a_an(veh_name), 
                 GET_VEH_DEFPOS(vehicle),
                 towing_comma ? ", " : " (",
                 GET_VEH_NAME(vehicle->towing),
                 towing_comma ? ")" : "");
      } else {
        snprintf(ENDOF(buf), sizeof(buf) - strlen(buf), "%s^y %s^y", 
               should_capitalize ? CAP(veh_name) : decapitalize_a_an(veh_name), 
               GET_VEH_DEFPOS(vehicle));
      }
    } else {
      switch (vehicle->cspeed) {
        case SPEED_OFF:
          if ((vehicle->type == VEH_BIKE && vehicle->people) || vehicle->restring) {
            snprintf(ENDOF(buf), sizeof(buf) - strlen(buf), "%s^y waits here", should_capitalize ? CAP(veh_name) : decapitalize_a_an(veh_name));
          } else {
            strlcat(buf, GET_VEH_ROOM_DESC(vehicle), sizeof(buf));
          }
          break;
        case SPEED_IDLE:
          snprintf(ENDOF(buf), sizeof(buf) - strlen(buf), "%s^y idles here", should_capitalize ? CAP(veh_name) : decapitalize_a_an(veh_name));
          break;
        case SPEED_CRUISING:
          if (veh_is_currently_flying(vehicle)) {
            snprintf(ENDOF(buf), sizeof(buf) - strlen(buf), "%s^y is airborne somewhere in the distance", should_capitalize ? CAP(veh_name) : decapitalize_a_an(veh_name));
          } else if (veh_can_traverse_air(vehicle)) {
            if (vehicle->type == VEH_DRONE) {
              snprintf(ENDOF(buf), sizeof(buf) - strlen(buf), "%s^y is flying around", should_capitalize ? CAP(veh_name) : decapitalize_a_an(veh_name));
            } else {
              snprintf(ENDOF(buf), sizeof(buf) - strlen(buf), "%s^y taxis around here", should_capitalize ? CAP(veh_name) : decapitalize_a_an(veh_name));
            }
          } else {
            snprintf(ENDOF(buf), sizeof(buf) - strlen(buf), "%s^y cruises through here", should_capitalize ? CAP(veh_name) : decapitalize_a_an(veh_name));
          }
          break;
        case SPEED_SPEEDING:
          snprintf(ENDOF(buf), sizeof(buf) - strlen(buf), "%s^y speeds past you", should_capitalize ? CAP(veh_name) : decapitalize_a_an(veh_name));
          break;
        case SPEED_MAX:
          snprintf(ENDOF(buf), sizeof(buf) - strlen(buf), "%s^y zooms by you", should_capitalize ? CAP(veh_name) : decapitalize_a_an(veh_name));
          break;
      }
      if (vehicle->towing) {
        snprintf(ENDOF(buf), sizeof(buf) - strlen(buf), ", towing %s^y", GET_VEH_NAME(vehicle->towing));
      }
      strlcat(buf, ".", sizeof(buf));
    }
  }

  if (vehicle->rigger) {
    snprintf(ENDOF(buf), sizeof(buf) - strlen(buf), " (Rigged by %s^y)", GET_CHAR_NAME(vehicle->rigger));
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

bool items_are_visually_similar(struct obj_data *first, struct obj_data *second) {
  if (!first || !second) {
    mudlog("SYSERR: Received null object to items_are_visually_similar.", NULL, LOG_SYSLOG, TRUE);
    return FALSE;
  }

  // Biggest litmus test: Are they even the same thing?
  if (first->item_number != second->item_number)
    return FALSE;

  // Quest items don't visually stack.
  if (GET_OBJ_QUEST_CHAR_ID(first) != GET_OBJ_QUEST_CHAR_ID(second))
    return FALSE;

  // Kept state must match.
  if (IS_OBJ_STAT(first, ITEM_EXTRA_KEPT) != IS_OBJ_STAT(second, ITEM_EXTRA_KEPT))
    return FALSE;

  // If the names don't match, they're not similar.
  if (strcmp(first->text.name, second->text.name))
    return FALSE;

  // If their restrings don't match...
  if ((first->restring && !second->restring) ||
      (!first->restring && second->restring) ||
      (first->restring && second->restring && strcmp(first->restring, second->restring)))
    return FALSE;

  #define COMPARE(field)  if (field(first) != field(second)) { return FALSE; }
  switch (GET_OBJ_TYPE(first)) {
    case ITEM_GUN_MAGAZINE:
      // If they're magazines, their various bonded stats must match too.
      COMPARE(GET_MAGAZINE_BONDED_MAXAMMO);
      COMPARE(GET_MAGAZINE_BONDED_ATTACKTYPE);
      break;
    case ITEM_GUN_AMMO:
      // Ammo boxes must match requisite fields.
      COMPARE(GET_AMMOBOX_INTENDED_QUANTITY);
      COMPARE(GET_AMMOBOX_WEAPON);
      COMPARE(GET_AMMOBOX_TYPE);
      break;
    case ITEM_DESIGN:
      // Design completion status.
      COMPARE(GET_DESIGN_COMPLETED);
      COMPARE(GET_DESIGN_DESIGNING_TICKS_LEFT);
      break;
    case ITEM_PROGRAM:
      // Cooked / uncooked status matters for programs.
      COMPARE(GET_PROGRAM_IS_COOKED);
      break;
    case ITEM_PART:
      // Designed / not designed.
      COMPARE(GET_PART_DESIGN_COMPLETION);
      break;
  }
  #undef COMPARE

  return TRUE;
}

void list_obj_to_char(struct obj_data * list, struct char_data * ch, int mode,
                      bool show, bool corpse)
{
  int num = 1, missed_items = 0;
  bool found;
  bool found_graffiti;

  found = FALSE;
  found_graffiti = FALSE;

  struct veh_data * in_veh = ch->in_veh;
  bool vfront = ch->vfront;

  if (IS_RIGGING(ch)) {
    struct veh_data *veh;
    RIG_VEH(ch, veh);
    in_veh = veh->in_veh;
    vfront = FALSE;
  }

  for (struct obj_data *i = list; i; i = i->next_content)
  {
    if ((i->in_veh && in_veh) && i->vfront != vfront)
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

        if (success_test(GET_INT(ch) + GET_POWER(ch, ADEPT_IMPROVED_PERCEPT), success_test_tn) <= 0) {
          missed_items++;
          continue;
        }
      }
    } else if (in_veh && i->in_room && i->in_room == in_veh->in_room) {
      if (in_veh->cspeed > SPEED_IDLE) {
        int success_test_tn;

        if (get_speed(in_veh) >= 200) {
          success_test_tn = 7;
        } else if (get_speed(in_veh) >= 120) {
          success_test_tn = 6;
        } else if (get_speed(in_veh) >= 60) {
          success_test_tn = 5;
        } else {
          success_test_tn = 4;
        }

        if (success_test(GET_INT(ch) + GET_POWER(ch, ADEPT_IMPROVED_PERCEPT), success_test_tn) <= 0) {
          missed_items++;
          continue;
        }
      }
    }

    while (i->next_content && (!(i->in_veh && in_veh) || (i->next_content->vfront == vfront))) {
      if (!items_are_visually_similar(i, i->next_content))
        break;

      if (CAN_SEE_OBJ(ch, i))
        num++;

      i = i->next_content;
    }

    if (CAN_SEE_OBJ(ch, i)) {
      if (corpse) {
        if (IS_OBJ_STAT(i, ITEM_EXTRA_CORPSE)) {
          if (num > 1) {
            send_to_char(ch, "(%d) ", num);
          }
          show_obj_to_char(i, ch, mode);
        }
      } else {
        if (IS_OBJ_STAT(i, ITEM_EXTRA_CORPSE)) {
          num = 1;
          continue;
        }

        if (!mode) {
          if (OBJ_IS_GRAFFITI(i) && (!found_graffiti) ) {
            found_graffiti = TRUE;
            send_to_char(ch, "^gSomeone has tagged the area:^n\r\n");
          }
          if (num > 1) {
            send_to_char(ch, "(%d) ", num);
          }
          show_obj_to_char(i, ch, mode);
        } else if (mode || ch->char_specials.rigging || in_veh) {
          if (num > 1) {
            send_to_char(ch, "(%d) ", num);
          }
          show_obj_to_char(i, ch, mode);
        }
      }
      found = TRUE;
      num = 1;
    }
  }

  if (!found) {
    if (missed_items) {
      send_to_char("You're moving too fast to see everything here!\r\n", ch);
    } else if (show) {
      send_to_char(" Nothing.\r\n", ch);
    }
  }
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

  if (IS_NPC(i) && MOB_FLAGGED(i, MOB_INANIMATE)) {
    if (phys >= 100 || (GET_TRADITION(i) == TRAD_ADEPT && phys >= 0 &&
                        ((100 - phys) / 10) <= GET_POWER(i, ADEPT_PAIN_RESISTANCE)))
      strlcat(buf, " is in excellent physical condition", sizeof(buf));
    else if (phys >= 90)
      strlcat(buf, " has a few scratches", sizeof(buf));
    else if (phys >= 75)
      strlcat(buf, " has some small dents and scrapes", sizeof(buf));
    else if (phys >= 50)
      strlcat(buf, " has quite a few dents", sizeof(buf));
    else if (phys >= 30)
      strlcat(buf, " has some big nasty dents and rents", sizeof(buf));
    else if (phys >= 15)
      strlcat(buf, " looks pretty damaged", sizeof(buf));
    else if (phys >= 0)
      strlcat(buf, " is in awful condition", sizeof(buf));
    else
      strlcat(buf, " is critically damaged", sizeof(buf));

    if (phys <= 0)
      strlcat(buf, " and is rebooting.\r\n", sizeof(buf));
    else if (ment >= 100 || (GET_TRADITION(i) == TRAD_ADEPT && ment >= 0 &&
                             ((100 - ment) / 10) <= (GET_POWER(i, ADEPT_PAIN_RESISTANCE) -
                                                     (int)((GET_MAX_PHYSICAL(i) - GET_PHYSICAL(i)) / 100))))
      strlcat(buf, " and is online.\r\n", sizeof(buf));
    else if (ment >= 90)
      strlcat(buf, " and is barely malfunctioning.\r\n", sizeof(buf));
    else if (ment >= 75)
      strlcat(buf, " and is showing minor issues.\r\n", sizeof(buf));
    else if (ment >= 50)
      strlcat(buf, " and is behaving erratically.\r\n", sizeof(buf));
    else if (ment >= 30)
      strlcat(buf, " and is glitching out.\r\n", sizeof(buf));
    else if (ment >= 10)
      strlcat(buf, " and is on the edge of a hard shutdown.\r\n", sizeof(buf));
    else
      strlcat(buf, " and is rebooting.\r\n", sizeof(buf));
  } else {
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
  }

  send_to_char(buf, ch);

  // Diagnose mortally-wounded PCs.
  if (!IS_NPC(i)) {
    if (phys <= 0) {
      bool has_metabolic_arrester = FALSE;

      if (AFF_FLAGS(i).IsSet(AFF_STABILIZE)) {
        send_to_char(ch, "%s'%s stabilized for the moment.\r\n", HSSH(i), !HSSH_SHOULD_PLURAL(i) ? "re" : "s");
        return;
      }

      // Find metabolic arrestor, if they have one.
      for (struct obj_data *obj = i->bioware; obj; obj = obj->next_content) {
        if (GET_BIOWARE_TYPE(obj) == BIO_METABOLICARRESTER) {
          has_metabolic_arrester = TRUE;
          break;
        }
      }

      int min_body = -GET_BOD(i) + (GET_BIOOVER(i) > 0 ? GET_BIOOVER(i) : 0);
      int boxes_left = (int)(GET_PHYSICAL(i) / 100) - min_body - 1;

      // Print info. Time is a best guess on my part, it seems to tick down every MUD hour?
      if (boxes_left <= 0) {
        send_to_char(ch, "%s %s near death! You estimate %s %s only %d minutes left to live.\r\n",
                     CAP(HSSH(i)),
                     !HSSH_SHOULD_PLURAL(i) ? "are" : "is",
                     HSSH(i),
                     !HSSH_SHOULD_PLURAL(i) ? "have" : "has",
                     2 * GET_PC_SALVATION_TICKS(i) * (has_metabolic_arrester ? 5 : 1));
      } else {
        send_to_char(ch, "You estimate %s %s %d minutes left to live.\r\n",
                     HSSH(i),
                     !HSSH_SHOULD_PLURAL(i) ? "have" : "has",
                     2 * (boxes_left + GET_PC_SALVATION_TICKS(i)) * (has_metabolic_arrester ? 5 : 1));
      }
    }
    if (LAST_HEAL(i) > 0) {
      send_to_char(ch, "%s%s received recent treatment, so further treatments will be more difficult (TN +%d).",
                       CAP(HSSH(i)),
                       !HSSH_SHOULD_PLURAL(i) ? "'ve" : "'s",
                       MIN(LAST_HEAL(i) * 3/2, 8));
    }
  }
}

const char *render_ware_for_viewer(struct obj_data *ware, bool privileged, bool force_full_name) {
  static char render_buf[1000];

  // Show its restring and what it was restrung from
  if (privileged && ware->restring) {
    if (GET_OBJ_VNUM(ware) == OBJ_CUSTOM_NERPS_BIOWARE || GET_OBJ_VNUM(ware) == OBJ_CUSTOM_NERPS_CYBERWARE) {
      snprintf(render_buf, sizeof(render_buf), "%s (NERPS)", ware->restring);
    } else {
      snprintf(render_buf, sizeof(render_buf), "%s (restrung from %s)", ware->restring, ware->text.name);
    }
  }
  // Show its restrung / full name.
  else if (privileged || force_full_name || ware->restring) {
    strlcpy(render_buf, GET_OBJ_NAME(ware), sizeof(render_buf));
  }
  // Only show its type.
  else {
    if (GET_OBJ_TYPE(ware) == ITEM_CYBERWARE)
      strlcpy(render_buf, cyber_types[GET_CYBERWARE_TYPE(ware)], sizeof(render_buf));
    else
      strlcpy(render_buf, bio_types[GET_BIOWARE_TYPE(ware)], sizeof(render_buf));
  }

  strlcat(render_buf, "\r\n", sizeof(render_buf));
  return render_buf;
}

void look_at_char(struct char_data * i, struct char_data * ch, const char *used_keyword)
{
  int j, found, weight;
  float height;
  struct obj_data *tmp_obj;

#ifdef USE_JANKY_EQUIPMENT_CONCEAL_SYSTEM
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
#endif
  {
    if (i->player.physical_text.look_desc)
      send_to_char(i->player.physical_text.look_desc, ch);
    else
      act("You see nothing special about $m.", FALSE, i, 0, ch, TO_VICT);

    if (!IS_NPC(i) && GET_LEVEL(ch) >= GET_LEVEL(i)) {
      act(get_lifestyle_string(i), FALSE, i, 0, ch, TO_VICT | SKIP_YOU_STANZAS);
      send_to_char("", ch);
    }

    if (used_keyword && *used_keyword)
      send_exdescs_on_look(ch, i, used_keyword);

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
                "appear%s to weigh about %d kg.", HSSH_SHOULD_PLURAL(i) ? "s" : "", height, HSSH_SHOULD_PLURAL(i) ? "s" : "", weight);
        act(buf, FALSE, i, 0, ch, TO_VICT);
      } else {
        snprintf(buf, sizeof(buf), "$e look%s to be about %0.1f meters tall and "
                "appear%s to barely weigh anything.", HSSH_SHOULD_PLURAL(i) ? "s" : "", height, HSSH_SHOULD_PLURAL(i) ? "s" : "");
        act(buf, FALSE, i, 0, ch, TO_VICT);
      }
    }
    diag_char_to_char(i, ch);
    if (SEES_ASTRAL(ch)) {
      bool dual = TRUE, init = TRUE;
      int grade = GET_GRADE(i);
      if (IS_OTAKU(i) && !PLR_FLAGGED(i, PLR_MATRIX)) grade = 0; // Otaku only show auras while decking, NERPs
      if (grade) {
        int targ = MAX(1, GET_GRADE(ch) - grade);
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
          if (grade > 20)
            strlcat(buf, "^Wblindingly bright^n.", sizeof(buf));
          else if (grade > 16)
            strlcat(buf, "^Wbrilliant^n.", sizeof(buf));
          else if (grade > 12)
            strlcat(buf, "^Wbright^n.", sizeof(buf));
          else if (grade > 8)
            strlcat(buf, "^Wa strong glow^n.", sizeof(buf));
          else if (grade > 4)
            strlcat(buf, "^Wglowing^n.", sizeof(buf));
          else strlcat(buf, "^Wa lambent glow^n.", sizeof(buf));
          strlcat(buf, "\r\n", sizeof(buf));
          send_to_char(buf, ch);
        }
      }
      if (IS_DUAL(i) && dual)
        send_to_char(ch, "%s %s dual.\r\n", CAP(HSSH(i)), ISARE(i));
    }
  }
  if (!IS_NPC(i) && GET_LEVEL(ch) > LVL_MORTAL && i->player.background)
    send_to_char(ch, "\r\n^cBackground:^n\r\n%s\r\n", i->player.background);
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
    for (j = 0; j < NUM_WEARS; j++) {
      struct obj_data *eq = GET_EQ(i, j);

      if (eq && CAN_SEE_OBJ(ch, eq)) {
        // You can see what's inside a large-weapon holster.
        if (GET_OBJ_TYPE(eq) == ITEM_HOLSTER && GET_HOLSTER_TYPE(eq) == HOLSTER_TYPE_LARGE_GUNS) {
          send_to_char(where[j], ch);
          show_obj_to_char(eq, ch, SHOW_MODE_OWN_EQUIPMENT);
          continue;
        } 
        
        // Special strings for wield / hold.
        if (j == WEAR_WIELD || j == WEAR_HOLD) {
          if (IS_OBJ_STAT(eq, ITEM_EXTRA_TWOHANDS))
            send_to_char(MOB_FLAGGED(i, MOB_INANIMATE) ? "<firmly mounted>     " : hands[2], ch);
          else if (j == WEAR_WIELD)
            send_to_char(MOB_FLAGGED(i, MOB_INANIMATE) ? "<mounted>            " : hands[(int)i->char_specials.saved.left_handed], ch);
          else
            send_to_char(MOB_FLAGGED(i, MOB_INANIMATE) ? "<mounted>            " : hands[!i->char_specials.saved.left_handed], ch);
          show_obj_to_char(eq, ch, SHOW_MODE_SOMEONE_ELSES_EQUIPMENT);
          continue;
        } 
        
        // Body/arm/waist slots are hidden by about unless you pass a check.
        if (   (j == WEAR_BODY || j == WEAR_LARM || j == WEAR_RARM || j == WEAR_WAIST)
            && GET_EQ(i, WEAR_ABOUT)) 
        {
          if (success_test(GET_INT(ch) + GET_POWER(ch, ADEPT_IMPROVED_PERCEPT), 4 + GET_WORN_CONCEAL_RATING(GET_EQ(i, WEAR_ABOUT))) >= 2) {
            send_to_char(where[j], ch);
            show_obj_to_char(eq, ch, SHOW_MODE_SOMEONE_ELSES_EQUIPMENT);
          }
          continue;
        }
        
        // Under-armor slot is hidden by about and body slots unless you pass a check.
        if (j == WEAR_UNDER && (GET_EQ(i, WEAR_ABOUT) || GET_EQ(i, WEAR_BODY))) {
          if (success_test(GET_INT(ch) + GET_POWER(ch, ADEPT_IMPROVED_PERCEPT), 6 +
                           (GET_EQ(i, WEAR_ABOUT) ? GET_WORN_CONCEAL_RATING(GET_EQ(i, WEAR_ABOUT)) : 0) +
                           (GET_EQ(i, WEAR_BODY) ? GET_WORN_CONCEAL_RATING(GET_EQ(i, WEAR_BODY)) : 0)) >= 2) {
            send_to_char(where[j], ch);
            show_obj_to_char(eq, ch, SHOW_MODE_SOMEONE_ELSES_EQUIPMENT);
          }
          continue;
        }
        
        // Legs are hidden by about unless you pass a check.
        if (j == WEAR_LEGS && GET_EQ(i, WEAR_ABOUT)) {
          if (success_test(GET_INT(ch) + GET_POWER(ch, ADEPT_IMPROVED_PERCEPT), 2 + GET_WORN_CONCEAL_RATING(GET_EQ(i, WEAR_ABOUT))) >= 2) {
            send_to_char(where[j], ch);
            show_obj_to_char(eq, ch, SHOW_MODE_SOMEONE_ELSES_EQUIPMENT);
          }
          continue;
        }
        
        // Ankle slots are hidden by about and legs unless passing a check.
        if ((j == WEAR_RANKLE || j == WEAR_LANKLE) && (GET_EQ(i, WEAR_ABOUT) || GET_EQ(i, WEAR_LEGS))) {
          if (success_test(GET_INT(ch) + GET_POWER(ch, ADEPT_IMPROVED_PERCEPT), 5 +
                           (GET_EQ(i, WEAR_ABOUT) ? GET_WORN_CONCEAL_RATING(GET_EQ(i, WEAR_ABOUT)) : 0) +
                           (GET_EQ(i, WEAR_LEGS) ? GET_WORN_CONCEAL_RATING(GET_EQ(i, WEAR_LEGS)) : 0)) >= 2) {
            send_to_char(where[j], ch);
            show_obj_to_char(eq, ch, SHOW_MODE_SOMEONE_ELSES_EQUIPMENT);
          }
          continue;
        }

        // Underwear (lower body) is hidden if you're wearing under/pants.
        if (j == WEAR_UNDERWEAR && (GET_EQ(i, WEAR_UNDER) || GET_EQ(i, WEAR_LEGS) || GET_EQ(i, WEAR_BODY) || GET_EQ(i, WEAR_ABOUT))) {
          continue;
        }

        // Chest slot (upper body underwear) is hidden by under/body.
        if (j == WEAR_CHEST && (GET_EQ(i, WEAR_UNDER) || GET_EQ(i, WEAR_BODY) || GET_EQ(i, WEAR_ABOUT))) {
          continue;
        }

        // Hidden focus (still visible to staff)
        if (ch != i && !IS_SENATOR(ch) && GET_OBJ_TYPE(eq) == ITEM_FOCUS && IS_OBJ_STAT(eq, ITEM_EXTRA_CONCEALED_IN_EQ)) {
          continue;
        }

        // Final case: Nothing hid it, show it.
        send_to_char(where[j], ch);
        show_obj_to_char(eq, ch, SHOW_MODE_SOMEONE_ELSES_EQUIPMENT);
      }
    }
  }

  char internal_ware[MAX_STRING_LENGTH];
  char visible_ware[MAX_STRING_LENGTH];
  *internal_ware = '\0';
  *visible_ware = '\0';

  bool ch_can_see_all_ware = (access_level(ch, LVL_FIXER) || (PRF_FLAGGED(i, PRF_HIRED) && PRF_FLAGGED(ch, PRF_QUESTOR)));

  for (tmp_obj = i->cyberware; tmp_obj; tmp_obj = tmp_obj->next_content) {
    bool ware_is_internal = TRUE, force_full_name = FALSE;

    switch (GET_CYBERWARE_TYPE(tmp_obj)) {
      case CYB_CUSTOM_NERPS:
        if (IS_SET(GET_CYBERWARE_FLAGS(tmp_obj), NERPS_WARE_VISIBLE)) {
          ware_is_internal = FALSE;
          force_full_name = TRUE;
        }
        break;
      case CYB_HANDRAZOR:
      case CYB_HANDBLADE:
      case CYB_HANDSPUR:
      case CYB_FIN:
      case CYB_FOOTANCHOR:
      case CYB_CLIMBINGCLAWS:
      case CYB_FANGS:
      case CYB_HORNS:
        if (!GET_CYBERWARE_IS_DISABLED(tmp_obj)) {
          ware_is_internal = FALSE;
        }
        break;
      // BUF2 is VISIBLE, buf is hidden
      case CYB_DATAJACK:
        if (!GET_EQ(i, WEAR_HEAD) && !GET_CYBERWARE_FLAGS(tmp_obj)) {
          ware_is_internal = FALSE;
        }
        break;
      case CYB_CHIPJACK:
        if (!GET_EQ(i, WEAR_HEAD)) {
          ware_is_internal = FALSE;
        }
        break;
      case CYB_DERMALPLATING:
      case CYB_BALANCETAIL:
        ware_is_internal = FALSE;
        break;
      case CYB_EYES:
        if (!ch_can_see_all_ware && (GET_EQ(i, WEAR_EYES) || (GET_EQ(i, WEAR_HEAD) && GET_WORN_CONCEAL_RATING(GET_EQ(i, WEAR_HEAD)) > 1)))
          continue;

        ware_is_internal = FALSE;

        if (IS_SET(GET_CYBERWARE_FLAGS(tmp_obj), EYE_COSMETIC))
          force_full_name = TRUE;

        // Optical magnification is shown separately, unless you're viewing in privileged mode.
        if (!ch_can_see_all_ware) {
          if ((IS_SET(GET_CYBERWARE_FLAGS(tmp_obj), EYE_OPTMAG1)
               || IS_SET(GET_CYBERWARE_FLAGS(tmp_obj), EYE_OPTMAG2)
               || IS_SET(GET_CYBERWARE_FLAGS(tmp_obj), EYE_OPTMAG3))
              && success_test(GET_INT(ch), 9) > 0)
          {
            strlcat(visible_ware, "Optical Magnification\r\n", sizeof(visible_ware));
          }
        }
        break;
      case CYB_ARMS:
      case CYB_LEGS:
      case CYB_SKULL:
      case CYB_TORSO:
        if ((GET_CYBERWARE_TYPE(tmp_obj) == CYB_ARMS && IS_SET(GET_CYBERWARE_FLAGS(tmp_obj), ARMS_MOD_OBVIOUS))
            || (GET_CYBERWARE_TYPE(tmp_obj) == CYB_LEGS && IS_SET(GET_CYBERWARE_FLAGS(tmp_obj), LEGS_MOD_OBVIOUS))
            || (GET_CYBERWARE_TYPE(tmp_obj) == CYB_SKULL && IS_SET(GET_CYBERWARE_FLAGS(tmp_obj), SKULL_MOD_OBVIOUS))
            || (GET_CYBERWARE_TYPE(tmp_obj) == CYB_TORSO && IS_SET(GET_CYBERWARE_FLAGS(tmp_obj), TORSO_MOD_OBVIOUS))
        ) {
          ware_is_internal = FALSE;
        }
        break;
    }

    // Now that we know if it's internal or not, hand it off to our render function.
    if (ware_is_internal) {
      if (ch_can_see_all_ware) {
        strlcat(internal_ware, render_ware_for_viewer(tmp_obj, ch_can_see_all_ware, force_full_name), sizeof(internal_ware));
      }
    } else {
      strlcat(visible_ware, render_ware_for_viewer(tmp_obj, ch_can_see_all_ware, force_full_name), sizeof(visible_ware));
    }
  }

  if (*visible_ware)
    send_to_char(ch, "\r\nVisible Cyberware:\r\n%s", visible_ware);
  if (*internal_ware)
    send_to_char(ch, "\r\nInternal Cyberware:\r\n%s", internal_ware);

  // Reset: It's bioware time.
  *visible_ware = '\0';
  *internal_ware = '\0';

  for (tmp_obj = i->bioware; tmp_obj; tmp_obj = tmp_obj->next_content) {
    bool ware_is_internal = TRUE;
    bool force_full_name = FALSE;

    if (GET_BIOWARE_TYPE(tmp_obj) == BIO_ORTHOSKIN) {
      int targ = 10;
      switch (GET_BIOWARE_RATING(tmp_obj)) {
        case 2:
          targ = 9;
          break;
        case 3:
          targ = 8;
      }

      if (ch_can_see_all_ware || tmp_obj->restring || success_test(GET_INT(ch), targ) > 0) {
        ware_is_internal = FALSE;
      }
    } else if (GET_BIOWARE_TYPE(tmp_obj) == BIO_CATSEYES) {
      ware_is_internal = FALSE;
    } else if (GET_BIOWARE_TYPE(tmp_obj) == BIO_CUSTOM_NERPS && IS_SET(GET_BIOWARE_FLAGS(tmp_obj), NERPS_WARE_VISIBLE)) {
      ware_is_internal = FALSE;
      force_full_name = TRUE;
    }

    // Now that we know if it's internal or not, hand it off to our render function.
    if (ware_is_internal) {
      if (ch_can_see_all_ware) {
        strlcat(internal_ware, render_ware_for_viewer(tmp_obj, ch_can_see_all_ware, force_full_name), sizeof(internal_ware));
      }
    } else {
      strlcat(visible_ware, render_ware_for_viewer(tmp_obj, ch_can_see_all_ware, force_full_name), sizeof(visible_ware));
    }
  }

  if (*visible_ware)
    send_to_char(ch, "\r\nVisible Bioware:\r\n%s", visible_ware);
  if (*internal_ware)
    send_to_char(ch, "\r\nInternal Bioware:\r\n%s", internal_ware);

  if (ch != i && (GET_REAL_LEVEL(ch) >= LVL_BUILDER || !AWAKE(i)))
  {
    found = FALSE;
    act("\r\nYou peek at $s inventory:", FALSE, i, 0, ch, TO_VICT);
    for (tmp_obj = i->carrying; tmp_obj; tmp_obj = tmp_obj->next_content) {
      if (CAN_SEE_OBJ(ch, tmp_obj)) {
        show_obj_to_char(tmp_obj, ch, SHOW_MODE_IN_INVENTORY);
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
  if (IS_NPC(i)
      && i->player.physical_text.room_desc
      && GET_POS(i) == GET_DEFAULT_POS(i)
      && !AFF_FLAGGED(i, AFF_PRONE)
      && !MOB_FLAGGED(i, MOB_FLAMEAURA))
  {
    *buf = '\0';

    // Note quest or nokill protection.
    if (
      (i->in_room && (GET_ROOM_SPEC(i->in_room) == mageskill_moore || GET_ROOM_SPEC(i->in_room) == mageskill_hermes))
      || CHECK_FUNC_AND_SFUNC_FOR(i, mageskill_herbie)
      || CHECK_FUNC_AND_SFUNC_FOR(i, mageskill_anatoly)
      || CHECK_FUNC_AND_SFUNC_FOR(i, mageskill_nightwing)
    ) {
      if (ch_has_obj_with_vnum(ch, OBJ_MAGE_LETTER))
        strlcat(buf, "^Y(Skill Quest)^n ", sizeof(buf));
    }
#ifdef USE_PRIVATE_CE_WORLD
    else if (
      CHECK_FUNC_AND_SFUNC_FOR(i, marksmanship_first)
      || CHECK_FUNC_AND_SFUNC_FOR(i, marksmanship_second)
      || CHECK_FUNC_AND_SFUNC_FOR(i, marksmanship_third)
      || CHECK_FUNC_AND_SFUNC_FOR(i, marksmanship_fourth)
      || CHECK_FUNC_AND_SFUNC_FOR(i, marksmanship_master)
    ) {
      if (ch_has_obj_with_vnum(ch, OBJ_MARKSMAN_LETTER)) {
        strlcat(buf, "^Y(Skill Quest)^n ", sizeof(buf));
      }
    }
#endif

    if (GET_MOB_QUEST_CHAR_ID(i)) {
      if (GET_MOB_QUEST_CHAR_ID(i) == GET_IDNUM_EVEN_IF_PROJECTING(ch)) {
        strlcat(buf, "^Y(Quest)^n ", sizeof(buf));
      } else if (!ch_is_blocked_by_quest_protections(ch, i)) {
        strlcat(buf, "^Y(Group Quest)^n ", sizeof(buf));
      } else {
        strlcat(buf, "^m(Protected)^n ", sizeof(buf));
      }
    }

    if (mob_is_alarmed_by_ch(i, ch) && (MOB_FLAGGED(i, MOB_HELPER) || MOB_FLAGGED(i, MOB_GUARD))) {
      strlcat(buf, "^r(alarmed)^n ", sizeof(buf));
    }

    // Make sure they always display flags that are relevant to the player.
    if (IS_AFFECTED(i, AFF_RUTHENIUM) || IS_AFFECTED(i, AFF_IMP_INVIS) || IS_AFFECTED(i, AFF_SPELLINVIS) || IS_AFFECTED(i, AFF_SPELLIMPINVIS))
      strlcat(buf, "(invisible) ", sizeof(buf));

    if (MOB_FLAGGED(i, MOB_FLAMEAURA) || affected_by_spell(i, SPELL_FLAME_AURA))
      strlcat(buf, "(flaming) ", sizeof(buf));

    if (IS_ASTRAL(ch) || IS_DUAL(ch)) {
      if (IS_ASTRAL(i))
        strlcat(buf, "(astral) ", sizeof(buf));
      else if (access_level(ch, LVL_BUILDER) && IS_PERCEIVING(i))
        strlcat(buf, "(perceiving) ", sizeof(buf));
      else if (IS_NPC(i) && IS_DUAL(i))
        strlcat(buf, "(dual) ", sizeof(buf));
    }
    
    if (AFF_FLAGGED(i, AFF_MANIFEST) && !SEES_ASTRAL(ch)) {
      snprintf(ENDOF(buf), sizeof(buf) - strlen(buf), "The ghostly image of %s", decapitalize_a_an(i->player.physical_text.room_desc));
    } else {
      strlcat(buf, CAP(i->player.physical_text.room_desc), sizeof(buf));
    }

    if (DISPLAY_HELPFUL_STRINGS_FOR_MOB_FUNCS) {
      bool already_printed = FALSE;
      if (mob_index[GET_MOB_RNUM(i)].func || mob_index[GET_MOB_RNUM(i)].sfunc) {
        if (MOB_HAS_SPEC(i, trainer)) {
          snprintf(ENDOF(buf), sizeof(buf) - strlen(buf), "^y...%s%s %s willing to train you.%s^n\r\n",
                   HSSH(i),
                   already_printed ? " also" : "",
                   HSSH_SHOULD_PLURAL(i) ? "looks" : "look",
                   SHOULD_SEE_TIPS(ch) ? " Use the ^YTRAIN^y command to begin." : "");
          already_printed = TRUE;
        }
        if (MOB_HAS_SPEC(i, teacher)) {
          if (MOB_FLAGGED(i, MOB_INANIMATE)) {
            snprintf(ENDOF(buf), sizeof(buf) - strlen(buf), "^y...it%s can be used to help you practice your skills.%s^n\r\n",
                     already_printed ? " also" : "",
                     SHOULD_SEE_TIPS(ch) ? " Use the ^YPRACTICE^y command to begin." : "");
          } else {
            snprintf(ENDOF(buf), sizeof(buf) - strlen(buf), "^y...%s%s %s willing to help you practice your skills.%s^n\r\n",
                     HSSH(i),
                     already_printed ? " also" : "",
                     HSSH_SHOULD_PLURAL(i) ? "looks" : "look",
                     SHOULD_SEE_TIPS(ch) ? " Use the ^YPRACTICE^y command to begin." : "");
          }
          already_printed = TRUE;
        }
        if (MOB_HAS_SPEC(i, metamagic_teacher)) {
          // Mundanes can't see metamagic teachers' abilities.
          if (GET_TRADITION(ch) != TRAD_MUNDANE) {
            snprintf(ENDOF(buf), sizeof(buf) - strlen(buf), "^y...%s%s %s willing to help you train in metamagic techniques.%s^n\r\n",
                     HSSH(i),
                     already_printed ? " also" : "",
                     HSSH_SHOULD_PLURAL(i) ? "looks" : "look",
                     SHOULD_SEE_TIPS(ch) ? " Use the ^YTRAIN^y command to begin." : "");
            already_printed = TRUE;
          }
        }
        if (MOB_HAS_SPEC(i, adept_trainer)) {
          // Adepts can't see adept trainers' abilities.
          if (GET_TRADITION(ch) == TRAD_ADEPT) {
            snprintf(ENDOF(buf), sizeof(buf) - strlen(buf), "^y...%s%s %s willing to help you train your powers.%s^n\r\n",
                     HSSH(i),
                     already_printed ? " also" : "",
                     HSSH_SHOULD_PLURAL(i) ? "looks" : "look",
                     SHOULD_SEE_TIPS(ch) ? " Use the ^YTRAIN^y command to begin." : "");
            already_printed = TRUE;
          }
          else if (!PLR_FLAGGED(ch, PLR_PAID_FOR_CLOSECOMBAT)) {
            snprintf(ENDOF(buf), sizeof(buf) - strlen(buf), "^y...%s%s %s able to help you train in martial arts.%s^n\r\n",
                     HSSH(i),
                     already_printed ? " also" : "",
                     HSSH_SHOULD_PLURAL(i) ? "looks" : "look",
                     SHOULD_SEE_TIPS(ch) ? " Use the ^YTRAIN^y command to begin." : "");
            already_printed = TRUE;
          }
        }
        if (MOB_HAS_SPEC(i, spell_trainer)) {
          // Mundanes and adepts can't see spell trainers' abilities.
          if (GET_TRADITION(ch) != TRAD_MUNDANE && GET_TRADITION(ch) != TRAD_ADEPT) {
            snprintf(ENDOF(buf), sizeof(buf) - strlen(buf), "^y...%s%s %s willing to help you learn new spells.%s^n\r\n",
                     HSSH(i),
                     already_printed ? " also" : "",
                     HSSH_SHOULD_PLURAL(i) ? "looks" : "look",
                     SHOULD_SEE_TIPS(ch) ? " Use the ^YLEARN^y command to begin." : "");
            already_printed = TRUE;
          }
        }
        if (MOB_HAS_SPEC(i, johnson)) {
          unsigned int max_rep = get_johnson_overall_max_rep(i);
          if (max_rep >= GET_REP(ch) || max_rep >= 10000) {
            snprintf(ENDOF(buf), sizeof(buf) - strlen(buf), "^y...%s%s might have a job for you.%s^n\r\n",
                     HSSH(i),
                     already_printed ? " also" : "",
                     SHOULD_SEE_TIPS(ch) ? " See ^YHELP JOB^y for instructions." : "");
          } else {
            snprintf(ENDOF(buf), sizeof(buf) - strlen(buf), "^y...%s%s only has work for less-experienced 'runners.^n\r\n",
                     HSSH(i),
                     already_printed ? " also" : "");
          }
          already_printed = TRUE;
        }
        if (MOB_HAS_SPEC(i, shop_keeper) || MOB_HAS_SPEC(i, terell_davis)) {
          snprintf(ENDOF(buf), sizeof(buf) - strlen(buf), "^y...%s%s %s a few things for sale.%s^n\r\n",
                   HSSH(i),
                   already_printed ? " also" : "",
                   HASHAVE(i),
                   SHOULD_SEE_TIPS(ch) ? " Use the ^YLIST^y command to see them." : "");
          already_printed = TRUE;
        }
        if (MOB_HAS_SPEC(i, pocsec_unlocker)) {
          snprintf(ENDOF(buf), sizeof(buf) - strlen(buf), "^y...%s%s can unlock pocket secretaries.%s^n\r\n",
                   HSSH(i),
                   already_printed ? " also" : "",
                   SHOULD_SEE_TIPS(ch) ? " Use the ^YUNLOCK^y command to begin." : "");
          already_printed = TRUE;
        }
        if (MOB_HAS_SPEC(i, soulbound_unbinder)) {
          snprintf(ENDOF(buf), sizeof(buf) - strlen(buf), "^y...%s%s can unbond soulbound keys.%s^n\r\n",
                   HSSH(i),
                   already_printed ? " also" : "",
                   SHOULD_SEE_TIPS(ch) ? " Use the ^YUNBOND^y command to begin." : "");
          already_printed = TRUE;
        }
        if (MOB_HAS_SPEC(i, landlord_spec)) {
          snprintf(ENDOF(buf), sizeof(buf) - strlen(buf), "^y...%s%s might have some rooms for lease.%s^n\r\n",
                   HSSH(i),
                   already_printed ? " also" : "",
                   SHOULD_SEE_TIPS(ch) ? " See ^YHELP APARTMENTS^y for details." : "");
          already_printed = TRUE;
        }
        if (MOB_HAS_SPEC(i, fence)) {
          snprintf(ENDOF(buf), sizeof(buf) - strlen(buf), "^y...%s%s might be willing to buy paydata from you.%s^n\r\n",
                   HSSH(i),
                   already_printed ? " also" : "",
                   SHOULD_SEE_TIPS(ch) ? " If you have paydata, ^YUNINSTALL^y it from your deck and then ^YSELL PAYDATA^y." : "");
          already_printed = TRUE;
        }
        if (MOB_HAS_SPEC(i, hacker)) {
          snprintf(ENDOF(buf), sizeof(buf) - strlen(buf), "^y...%s%s cracks credsticks-- you can ^YGIVE^y them to %s.^n\r\n",
                   HSSH(i),
                   already_printed ? " also" : "",
                   HMHR(i));
          already_printed = TRUE;
        }
        if (MOB_HAS_SPEC(i, receptionist)) {
          snprintf(ENDOF(buf), sizeof(buf) - strlen(buf), "^y...%s%s %s bunks for rent.%s^n\r\n",
                   HSSH(i),
                   already_printed ? " also" : "",
                   HASHAVE(i),
                   SHOULD_SEE_TIPS(ch) ? " Renting is purely for flavor and is not required in AwakeMUD." : "");
          already_printed = TRUE;
        }
        if (MOB_HAS_SPEC(i, fixer)) {
          snprintf(ENDOF(buf), sizeof(buf) - strlen(buf), "^y...%s%s can repair objects for you.%s^n\r\n",
                   HSSH(i),
                   already_printed ? " also" : "",
                   SHOULD_SEE_TIPS(ch) ? " Use the ^YREPAIR^y command to proceed." : "");
          already_printed = TRUE;
        }
        if (i->in_room && GET_ROOM_SPEC(i->in_room) == mageskill_hermes) {
          for (int qidx = 0; qidx < QUEST_TIMER; qidx++) {
            if (GET_CQUEST(ch, qidx) == QST_MAGE_INTRO) {
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
        if (MOB_HAS_SPEC(i, marksmanship_master)) {
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
  strlcat(buf, "^n", sizeof(buf));

  if (GET_MOB_QUEST_CHAR_ID(i)) {
    if (GET_MOB_QUEST_CHAR_ID(i) == GET_IDNUM_EVEN_IF_PROJECTING(ch)) {
      strlcat(buf, " ^Y(Quest)^n", sizeof(buf));
    } else if (!ch_is_blocked_by_quest_protections(ch, i)) {
      strlcat(buf, " ^Y(Group Quest)^n", sizeof(buf));
    } else {
      strlcat(buf, " ^m(Protected)^n", sizeof(buf));
    }
  }

  if (mob_is_alarmed_by_ch(i, ch) && (MOB_FLAGGED(i, MOB_HELPER) || MOB_FLAGGED(i, MOB_GUARD))) {
    strlcat(buf, " ^r(alarmed)^n", sizeof(buf));
  }

  if (PRF_FLAGGED(i, PRF_AFK))
    strlcat(buf, " (AFK)", sizeof(buf));
  if (PLR_FLAGGED(i, PLR_SWITCHED))
    strlcat(buf, " (switched)", sizeof(buf));
  if (IS_AFFECTED(i, AFF_RUTHENIUM) || IS_AFFECTED(i, AFF_IMP_INVIS) || IS_AFFECTED(i, AFF_SPELLINVIS) || IS_AFFECTED(i, AFF_SPELLIMPINVIS))
    strlcat(buf, " (invisible)", sizeof(buf));
  if (!IS_NPC(i)) {
    // Always display the Staff identifier.
    if (IS_SENATOR(i))
      strlcat(buf, " (staff)", sizeof(buf));
    // Display the Player identifier for newbies.
    else if (PLR_FLAGGED(ch, PLR_NEWBIE))
      strlcat(buf, " (player)", sizeof(buf));
  }
  if (!IS_NPC(i) && !i->desc &&
      !PLR_FLAGS(i).AreAnySet(PLR_MATRIX, PLR_PROJECT, PLR_SWITCHED, ENDBIT))
    strlcat(buf, " (linkless)", sizeof(buf));
  if (SEES_ASTRAL(ch)) {
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
    if (dual) {
      if (access_level(ch, LVL_BUILDER) && IS_PERCEIVING(i))
        strlcat(buf, " (perceiving)", sizeof(buf));
      if (IS_DUAL(i))
        strlcat(buf, " (dual)", sizeof(buf));
    }

  }
  if (PLR_FLAGGED(i, PLR_WRITING))
    strlcat(buf, " (writing)", sizeof(buf));
  if (PLR_FLAGGED(i, PLR_MAILING))
    strlcat(buf, " (mailing)", sizeof(buf));
  if (PLR_FLAGGED(i, PLR_EDITING))
    strlcat(buf, " (editing)", sizeof(buf));
  if (PLR_FLAGGED(i, PLR_PROJECT))
    strlcat(buf, " (projecting)", sizeof(buf));

  if (GET_QUI(i) <= 0) {
    strlcat(buf, " is here, seemingly paralyzed.", sizeof(buf));
  } else if (GET_POS(i) == POS_FIGHTING) {
    if (FIGHTING(i)) {
      if (AFF_FLAGGED(ch, AFF_BANISH))
        strlcat(buf, " is here, attempting to banish ", sizeof(buf));
      else
        strlcat(buf, " is here, fighting ", sizeof(buf));
      if (FIGHTING(i) == ch)
        strlcat(buf, "YOU!", sizeof(buf));
      else {
        if (i->in_room == FIGHTING(i)->in_room)
          strlcat(buf, decapitalize_a_an(PERS(FIGHTING(i), ch)), sizeof(buf));
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
  } else if (PLR_FLAGGED(i, PLR_MATRIX))
    strlcat(buf, " is here, jacked into a cyberdeck.", sizeof(buf));
  else if (PLR_FLAGGED(i, PLR_REMOTE))
    strlcat(buf, " is here, jacked into a remote control deck.", sizeof(buf));
  else if (AFF_FLAGGED(i, AFF_DESIGN) || AFF_FLAGGED(i, AFF_PROGRAM))
    strlcat(buf, " is here, typing away at a computer.", sizeof(buf));
  else if (AFF_FLAGGED(i, AFF_PART_DESIGN) || AFF_FLAGGED(i, AFF_PART_BUILD))
    strlcat(buf, " is here, working on a cyberdeck.", sizeof(buf));
  else if (AFF_FLAGGED(i, AFF_SPELLDESIGN))
    strlcat(buf, " is here, working on a spell design.", sizeof(buf));
  else if (AFF_FLAGGED(i, AFF_COMPLEX_FORM_PROGRAM))
    strlcat(buf, " is here, meditating on a complex form.", sizeof(buf));
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
  else if (AFF_FLAGGED(i, AFF_PRONE)) {
    struct obj_data *wielded = GET_EQ(i, WEAR_WIELD);
    bool is_using_bipod_or_tripod = FALSE;

    if (wielded && WEAPON_IS_GUN(wielded)) {
      rnum_t rnum;

      for (int access_loc = ACCESS_LOCATION_TOP; access_loc <= ACCESS_LOCATION_UNDER; access_loc++) {
        if (GET_WEAPON_ATTACH_LOC(wielded, access_loc) <= 0)
          continue;

        if ((rnum = real_object(GET_WEAPON_ATTACH_LOC(wielded, access_loc))) > -1) {
          if (GET_OBJ_TYPE(&obj_proto[rnum]) == ITEM_GUN_ACCESSORY
              && (GET_ACCESSORY_TYPE(&obj_proto[rnum]) == ACCESS_BIPOD || GET_ACCESSORY_TYPE(&obj_proto[rnum]) == ACCESS_TRIPOD))
          {
            is_using_bipod_or_tripod = TRUE;
          }
        }
      }
    }

    if (is_using_bipod_or_tripod) {
      snprintf(ENDOF(buf), sizeof(buf) - strlen(buf), " is prone here, manning %s.", decapitalize_a_an(GET_OBJ_NAME(GET_EQ(i, WEAR_WIELD))));
    } else {
      strlcat(buf, " is lying prone here.", sizeof(buf));
    }
  }
  else if (AFF_FLAGGED(i, AFF_PILOT))
  {
    if (AFF_FLAGGED(i, AFF_RIG))
      strlcat(buf, " is plugged into the dashboard.", sizeof(buf));
    else
      strlcat(buf, " is sitting in the drivers seat.", sizeof(buf));
  } else if ((obj = get_mount_manned_by_ch(i))) {
      snprintf(ENDOF(buf), sizeof(buf) - strlen(buf), " is manning %s.", GET_OBJ_NAME(obj));
  } else if (affected_by_spell(i, SPELL_LEVITATE)) {
    strlcat(buf, " is here, hovering above the ground.", sizeof(buf));
  } else {
    if (GET_DEFPOS(i))
      snprintf(buf2, sizeof(buf2), " %s^n", GET_DEFPOS(i));
    else
      snprintf(buf2, sizeof(buf2), " %s^n.", positions[(int) GET_POS(i)]);
    strlcat(buf, buf2, sizeof(buf));
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
      if (CAN_SEE(ch, i) && ch != i && ch->vfront == i->vfront && !IS_IGNORING(ch, is_blocking_ic_interaction_from, i)) {
        list_one_char(i, ch);
      }
    }
  }

  // Show room's characters to character. Done this way because list_char_to_char should have been split for vehicles but wasn't.
  for (i = list; i; i = i->next_in_room) {
    // Skip them if they're invisible to us, or if they're us and we're not rigging.
    // TODO: Does this cause double printing if you're inside a nested vehicle and looking out at someone in the containing veh?
    if (!CAN_SEE(ch, i) || (!ch->char_specials.rigging && ch == i) || (ch->in_veh && i->in_veh == ch->in_veh)) {
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

    if (!IS_IGNORING(ch, is_blocking_ic_interaction_from, i))
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
        snprintf(buf2, sizeof(buf2), "%-5s - [%5ld] %s%s%s%s%s\r\n", dirs[door],
                 EXIT(ch, door)->to_room->number,
                 replace_neutral_color_codes(EXIT(ch, door)->to_room->name, autom ? "^c" : "^n"),
                 IS_SET(EXIT(ch, door)->exit_info, EX_LOCKED) ? " (locked)" : ((IS_SET(EXIT(ch, door)->exit_info, EX_CLOSED)) ? " (closed)" : ""),
                 IS_SET(EXIT(ch, door)->exit_info, EX_HIDDEN) ? " (hidden)" : "",
                 (veh && !room_accessible_to_vehicle_piloted_by_ch(EXIT(veh, door)->to_room, veh, ch, FALSE)) ? " (impassible)" : "",
                 IS_WATER(EXIT(ch, door)->to_room) ? " (flooded)" : ""
                );
        if (autom)
          strlcat(buf, "^c", sizeof(buf));
        strlcat(buf, CAP(buf2), sizeof(buf));
      } else if (!IS_SET(EXIT(ch, door)->exit_info, EX_HIDDEN)) {
        snprintf(buf2, sizeof(buf2), "%-5s - ", dirs[door]);
        if (!SEES_ASTRAL(ch) && !LIGHT_OK(ch))
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
          } else {
            strlcat(buf2, replace_neutral_color_codes(EXIT(ch, door)->to_room->name, autom ? "^c" : "^n"), sizeof(buf2));
            if (IS_WATER(EXIT(ch, door)->to_room))
              strlcat(buf2, " (flooded)", sizeof(buf2));
          }
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

void disp_short_exits(struct char_data *ch) {
  struct veh_data *veh;
  *buf = '\0';

  for (int door = 0; door < NUM_OF_DIRS; door++)
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

void do_auto_exits(struct char_data * ch)
{
  // This is SUCH a hack, but the down exit in 'airborne' should only show if you're not in a vehicle.
  if (ch->in_veh && ch->in_room && GET_ROOM_VNUM(ch->in_room) == RM_AIRBORNE) {
    send_to_char("^cObvious exits:\r\n", ch);
    send_to_char(" None.\r\n", ch);
    return;
  }

  if (PRF_FLAGGED(ch, PRF_LONGEXITS)) {
    disp_long_exits(ch, TRUE);
  }
  else {
    disp_short_exits(ch);
  }
}


ACMD(do_exits)
{
  if (subcmd == SCMD_LONGEXITS)
    disp_long_exits(ch, FALSE);
  else
    disp_short_exits(ch);
}

ACMD(do_mobs) {
  struct room_data *room = get_ch_in_room(ch);
  const char *location_string = ch->in_veh ? "around your vehicle" : "around you";

  if (IS_RIGGING(ch)) {
    struct veh_data *veh;
    RIG_VEH(ch, veh);
    room = get_veh_in_room(veh);
    location_string = "around your vehicle";
  }

  if (!room) {
    send_to_char("You can't see anything!!\r\n", ch);
    mudlog_vfprintf(ch, LOG_SYSLOG, "SYSERR: %s%s has no room in do_mobs!", GET_CHAR_NAME(ch), IS_RIGGING(ch) ? " (rigging)" : "");
    return;
  }

  bool printed_something = FALSE;
  for (struct char_data *mob = room->people; mob; mob = mob->next_in_room) {
    if (IS_NPC(mob) && CAN_SEE(ch, mob)) {
      if (!printed_something) {
        printed_something = TRUE;
        send_to_char(ch, "%s, you see:\r\n", CAP(location_string));
      }
      send_to_char(ch, "  %s\r\n", GET_CHAR_NAME(mob));
    }
  }

  if (!printed_something)
    send_to_char(ch, "You don't see any mobs %s.\r\n", location_string);
}

ACMD(do_items) {
  struct veh_data *in_veh = ch->in_veh;
  bool vfront = ch->vfront;
  struct room_data *in_room = ch->in_room;

  if (IS_RIGGING(ch)) {
    struct veh_data *veh;
    RIG_VEH(ch, veh);
    in_veh = veh->in_veh;
    in_room = veh->in_room;
    vfront = FALSE;
  }

  if (!in_room && !in_veh) {
    send_to_char("You can't see anything!!\r\n", ch);
    mudlog_vfprintf(ch, LOG_SYSLOG, "SYSERR: %s%s has no room OR veh in do_items!", GET_CHAR_NAME(ch), IS_RIGGING(ch) ? " (rigging)" : "");
    return;
  }

  bool printed_something = FALSE;
  for (struct obj_data *obj = in_veh ? in_veh->contents : in_room->contents; obj; obj = obj->next_content) {
    if (CAN_SEE_OBJ(ch, obj) && (!in_veh || obj->vfront == vfront)) {
      int num = 1;

      while (obj->next_content && (!in_veh || obj->next_content->vfront == vfront)) {
        if (!items_are_visually_similar(obj, obj->next_content))
          break;

        if (CAN_SEE_OBJ(ch, obj))
          num++;

        obj = obj->next_content;
      }

      if (!printed_something) {
        printed_something = TRUE;
        send_to_char("Around you, you see:\r\n", ch);
      }

      if (num > 1) {
        send_to_char(ch, "  (%d) %s\r\n", num, GET_OBJ_NAME(obj));
      } else {
        send_to_char(ch, "  %s\r\n", GET_OBJ_NAME(obj));
      }
    }
  }

  if (!printed_something)
    send_to_char(ch, "You don't see any items here.\r\n");
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
          snprintf(buf, sizeof(buf), "%s the blood staining this area washes away with the precipitation.", world[i].blood > 3 ? "Some of" : (world[i].blood > 1 ? "Most of" : "The last of"));
          send_to_room(buf, &world[i]);
        } else if (weather_info.sky == SKY_LIGHTNING) {
          world[i].blood -= 2;
          snprintf(buf, sizeof(buf), "%s the blood staining this area washes away with the heavy precipitation.", world[i].blood > 4 ? "Some of" : (world[i].blood > 1 ? "Most of" : "The last of"));
          send_to_room(buf, &world[i]);
        }
      }
    }
  }
}

void look_in_veh(struct char_data * ch)
{
  struct room_data *was_in = NULL;
  if (!IS_RIGGING(ch))
  {
    if (!ch->in_veh->in_room && !ch->in_veh->in_veh) {
      snprintf(buf, sizeof(buf), "SYSERR: Character %s is not in a room or vehicle.", GET_CHAR_NAME(ch));
      mudlog(buf, ch, LOG_SYSLOG, TRUE);
      send_to_char("ALERT AN IMM!!\r\n", ch);
      return;
    }
    send_to_char(ch, "^CInside %s^n\r\n", GET_VEH_NAME(ch->in_veh), ch);
    if (ch->vfront) {
      send_to_char(ch->in_veh->decorate_front ? ch->in_veh->decorate_front : ch->in_veh->inside_description, ch);
    } else {
      send_to_char(ch->in_veh->decorate_rear ? ch->in_veh->decorate_rear : ch->in_veh->rear_description, ch);
    }
    CCHAR = "^g";
    CGLOB = KGRN;
    // Show non-corpses.
    list_obj_to_char(ch->in_veh->contents, ch, SHOW_MODE_ON_GROUND, FALSE, FALSE);
    CGLOB = KNRM;
    CCHAR = NULL;
    // Show corpses.
    list_obj_to_char(ch->in_veh->contents, ch, SHOW_MODE_ON_GROUND, FALSE, TRUE);
    list_char_to_char(ch->in_veh->people, ch);
    if (veh_is_currently_flying(ch->in_veh)) {
      send_flight_estimate(ch, ch->in_veh);
    }
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
      // Show non-corpses.
      list_obj_to_char(veh->in_veh->contents, ch, SHOW_MODE_ON_GROUND, FALSE, FALSE);
      CGLOB = KNRM;
      CCHAR = NULL;
      // Show corpses.
      list_obj_to_char(veh->in_veh->contents, ch, SHOW_MODE_ON_GROUND, FALSE, TRUE);
      list_char_to_char(veh->in_veh->people, ch);
      ch->vfront = ov;
      CCHAR = "^y";
      list_veh_to_char(veh->in_veh->carriedvehs, ch);
    } else {
      display_room_name(ch, veh->in_room, TRUE);

      if (get_speed(veh) <= 200) {
        send_to_char(get_room_desc(veh->in_room), ch);
      } else {
        send_to_char("Your surroundings blur past.\r\n", ch);
      }
      if (veh_is_currently_flying(veh)) {
        send_flight_estimate(ch, veh);
      }
      if (PLR_FLAGGED(ch, PLR_REMOTE))
        was_in = ch->in_room;
      ch->in_room = veh->in_room;
      do_auto_exits(ch);
      CCHAR = "^g";
      CGLOB = KGRN;
      // Show non-corpses.
      list_obj_to_char(veh->in_room->contents, ch, SHOW_MODE_ON_GROUND, FALSE, FALSE);
      CGLOB = KNRM;
      CCHAR = NULL;
      // Show corpses.
      list_obj_to_char(veh->in_room->contents, ch, SHOW_MODE_ON_GROUND, FALSE, TRUE);
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

void look_at_room(struct char_data * ch, int ignore_brief, int is_quicklook)
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
    // They're neither in a vehicle nor room, whoops.
    mudlog_vfprintf(ch, LOG_SYSLOG, "SYSERR: %s tried to look at their room but is nowhere! Sending to 0.", GET_CHAR_NAME(ch));

    // Emergency rescue attempt.
    char_to_room(ch, &world[0]);
    
    if (!ch->in_room) {
      // Something SERIOUSLY fucked up.
      send_to_char("Your mind is blasted by the eldritch horrors of the unknown void you're drifting in.\r\n", ch);
      return;
    }
  }

  // Room title.
  display_room_name(ch, ch->in_room, FALSE);

  // TODO: Why is this code here? If you're in a vehicle, you do look_in_veh() above right?
  if (!(ch->in_veh && get_speed(ch->in_veh) > 200)) {
    if (!is_quicklook && (ignore_brief || !PRF_FLAGGED(ch, PRF_BRIEF))) {
      display_room_desc(ch);
    }
  }

  /* autoexits */
  if (PRF_FLAGGED(ch, PRF_AUTOEXIT))
    do_auto_exits(ch);

  if (ch->in_room->blood > 0)
    send_to_char(blood_messages[(int) ch->in_room->blood], ch);

  if (ch->in_room->debris > 0) {
    strncpy(buf, "^t", sizeof(buf));

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

  if (GET_BACKGROUND_COUNT(ch->in_room) && SEES_ASTRAL(ch)) {
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
      switch (GET_BACKGROUND_COUNT(ch->in_room)) {
        case 1:
          snprintf(buf, sizeof(buf), "%sA distracting", GET_BACKGROUND_AURA(ch->in_room) == AURA_WRONGNESS ? "^[F211]" : "^t");
          break;
        case 2:
          snprintf(buf, sizeof(buf), "%sA light", GET_BACKGROUND_AURA(ch->in_room) == AURA_WRONGNESS ? "^[F212]" : "^T");
          break;
        case 3:
          snprintf(buf, sizeof(buf), "%sA heavy", GET_BACKGROUND_AURA(ch->in_room) == AURA_WRONGNESS ? "^[F213]" : "^o");
          break;
        case 4:
          snprintf(buf, sizeof(buf), "%sAn almost overwhelming", GET_BACKGROUND_AURA(ch->in_room) == AURA_WRONGNESS ? "^[F214]" : "^O");
          break;
        case 5:
          snprintf(buf, sizeof(buf), "%sAn overwhelming", GET_BACKGROUND_AURA(ch->in_room) == AURA_WRONGNESS ? "^[F215]" : "^r");
          break;
      }
      snprintf(ENDOF(buf), sizeof(buf) - strlen(buf), " aura of %s pervades the area.^n\r\n", background_types[GET_BACKGROUND_AURA(ch->in_room)]);
      send_to_char(buf, ch);
    } else {
      send_to_char(ch, "^RThe mana is warped by %s here!^n\r\n", background_types[GET_BACKGROUND_AURA(ch->in_room)]);
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

  if (!PRF_FLAGGED(ch, PRF_NO_WEATHER)) {
    // Display lighting info.
    bool is_nighttime = (time_info.hours <= 6 || time_info.hours >= 19) && !ROOM_FLAGGED(ch->in_room, ROOM_INDOORS);
    if (is_nighttime && ROOM_FLAGGED(ch->in_room, ROOM_STREETLIGHTS)) {
      if (ROOM_FLAGS(ch->in_room).AreAnySet(ROOM_RUNWAY, ROOM_HELIPAD, ROOM_AIRCRAFT_CAN_DRIVE_HERE, ENDBIT)) {
        send_to_char("^LBright lights drive back the nighttime darkness.^n\r\n", ch);
      } else if (GET_JURISDICTION(ch->in_room) == JURISDICTION_SECRET) {
        if (ROOM_FLAGGED(ch->in_room, ROOM_ROAD)) {
          send_to_char("^LFlickering streetlights struggle amidst the nighttime darkness.^n\r\n", ch);
        } else {
          send_to_char("^LFlickering lights barely make a dent in the gloom.^n\r\n", ch);
        }
      } else if (ROOM_FLAGGED(ch->in_room, ROOM_ROAD)) {
        send_to_char("^LStreetlights drive back the nighttime darkness.^n\r\n", ch);
      } else {
        send_to_char("^LLights push back the nighttime darkness.^n\r\n", ch);
      }
    } else if (is_nighttime && ch->in_room->sector_type == SPIRIT_CITY && GET_JURISDICTION(ch->in_room) != JURISDICTION_SECRET) {
      send_to_char("^LStreaks of light pollution soften the shadows.^n\r\n", ch);
    } else if (is_nighttime || (ch->in_room->vision[0] > LIGHT_NORMAL && ch->in_room->vision[0] <= LIGHT_PARTLIGHT)) {
      if (ch->in_room->light[ROOM_HIGHEST_SPELL_FORCE]) {
        send_to_char(ch, "^LAn ambient magical glow lightens the %sshadows.^n\r\n", is_nighttime ? "nighttime " : "");
      } else if (ch->in_room->light[ROOM_LIGHT_HEADLIGHTS_AND_FLASHLIGHTS]) {
        if (ch->in_room->light[ROOM_LIGHT_HEADLIGHTS_AND_FLASHLIGHTS] == 1) {
          if (GET_EQ(ch, WEAR_LIGHT)) {
            send_to_char(ch, "^LYour flashlight highlights the %sshadows.^n\r\n", is_nighttime ? "nighttime " : "");
          } else {
            send_to_char(ch, "^LA beam of light highlights the %sshadows.^n\r\n", is_nighttime ? "nighttime " : "");
          }
        } else {
          send_to_char(ch, "^LSeveral beams of light highlight the %sshadows.^n\r\n", is_nighttime ? "nighttime " : "");
        }
      } else {
        switch (light_level(ch->in_room)) {
          case LIGHT_FULLDARK:
            if (GET_JURISDICTION(ch->in_room) == JURISDICTION_SECRET)
              send_to_char("^LA stygian gloom shrouds the area.^n\r\n", ch);
            else
              send_to_char(ch, "^LDarkness cloaks the area.^n\r\n");
            break;
          case LIGHT_MINLIGHT:
            if (GET_JURISDICTION(ch->in_room) == JURISDICTION_SECRET)
              send_to_char("^LDarkness lays heavily here, barely broken by faint lights.^n\r\n", ch);
            else
              send_to_char(ch, "^LDarkness cloaks the area, barely broken by faint lights.^n\r\n");
            break;
          case LIGHT_PARTLIGHT:
            if (GET_JURISDICTION(ch->in_room) == JURISDICTION_SECRET)
              send_to_char("^LShadows cling stubbornly, leaving deep pools of darkness about.^n\r\n", ch);
            else
              send_to_char(ch, "^LShadows cling to the area, making it harder to see.^n\r\n");
            break;
        }
      }
    }

    if (!ROOM_FLAGGED(ch->in_room, ROOM_INDOORS)) {
      bool should_be_snowy = precipitation_is_snow(GET_JURISDICTION(ch->in_room));

      // Display weather info in the room.
      if (IS_WATER(ch->in_room)) {
        if (weather_info.sky == SKY_RAINING) {
          if (should_be_snowy) {
            send_to_char(ch, "^WThe snow swirls about you as you swim.^n\r\n");
          } else {
            send_to_char(ch, "^cThe %srain gets in your eyes as you swim.^n\r\n", GET_JURISDICTION(ch->in_room) == JURISDICTION_SECRET ? "acrid " : "");
          }
        } else if (weather_info.sky == SKY_LIGHTNING) {
          if (should_be_snowy) {
            send_to_char(ch, "^WHeavy snowfall makes the water treacherous.^n\r\n");
          } else {
            send_to_char(ch, "^cThe %swater is made treacherous by the pounding rain.^n\r\n", GET_JURISDICTION(ch->in_room) == JURISDICTION_SECRET ? "polluted " : "");
          }
        }
      } else {
        if (weather_info.sky == SKY_RAINING) {
          if (ch->in_veh) {
            if (ch->in_veh->type == VEH_BIKE) {
              if (should_be_snowy) {
                send_to_char(ch, "^WSnowflakes tumble and fall about your bike.^n\r\n");
              } else {
                send_to_char(ch, "^c%s ricochets off your shoulders and splashes about the bike.^n\r\n", GET_JURISDICTION(ch->in_room) == JURISDICTION_SECRET ? "Stinking rain" : "Rain");
              }
            } else {
              if (should_be_snowy) {
                send_to_char(ch, "^WSnowflakes linger briefly on your windows.^n\r\n");
              } else {
                send_to_char(ch, "^c%s glides down your windows, wipers brushing it away with patient motion.^n\r\n", GET_JURISDICTION(ch->in_room) == JURISDICTION_SECRET ? "Oily rain" : "Rain");
              }
            }
          } else {
            if (should_be_snowy) {
              send_to_char(ch, "^WSnow drifts from the sky.^n\r\n");
            } else {
              send_to_char(ch, "^c%s splashes into the puddles around your feet.^n\r\n", GET_JURISDICTION(ch->in_room) == JURISDICTION_SECRET ? "Acrid rain" : "Rain");
            }
          }
        }
        else if (weather_info.sky == SKY_LIGHTNING) {
          if (ch->in_veh) {
            if (ch->in_veh->type == VEH_BIKE) {
              if (should_be_snowy) {
                send_to_char(ch, "^WHeavy snow whips roughly around you, clinging to your bike.^n\r\n");
              } else {
                send_to_char(ch, "^cHeavy rain %s you, running in rivulets off of your bike.^n\r\n", GET_JURISDICTION(ch->in_room) == JURISDICTION_SECRET ? "rips at" : "pounds down around");
              }
            } else {
              if (should_be_snowy) {
                send_to_char(ch, "^WHeavy snow flurries against your windows, making it hard to see.^n\r\n");
              } else {
                send_to_char(ch, "^cHeavy rain %s against your windows, making it hard to see.^n\r\n", GET_JURISDICTION(ch->in_room) == JURISDICTION_SECRET ? "slams" : "pounds");
              }
            }
          } else {
            if (should_be_snowy) {
              send_to_char(ch, "^WYou struggle to see through the heavy snowfall.^n\r\n");
            } else {
              send_to_char(ch, "^cYou struggle to see through the heavy rain that %s down from the sky.^n\r\n", GET_JURISDICTION(ch->in_room) == JURISDICTION_SECRET ? "pounds" : "sheets");
            }
          }
        }
        else if (weather_info.lastrain < 5) {
          if (should_be_snowy) {
            send_to_char(ch, "^WSnow has accumulated in small drifts here and there.^n\r\n");
          } else {
            send_to_char(ch, "^cThe ground is wet%s, it must have rained recently.^n\r\n", GET_JURISDICTION(ch->in_room) == JURISDICTION_SECRET ? " and slightly cleaner" : "");
          }
        }
        else if (weather_info.lastrain < 10 && should_be_snowy) {
          send_to_char(ch, "^W%s snow clings stubbornly in the shadows.^n\r\n", GET_JURISDICTION(ch->in_room) == JURISDICTION_SECRET ? "Dirty" : "Slushy");
        }
      }
    }
  }

  if (ch->in_room->poltergeist[0] > 0) {
    if (!number(0, 10000)) {
      // Poultrygeist joke line.  Don't @ me about this.
      send_to_char("^cAn invisible force is tumbling chickens about in a ghostly manner.^n\r\n", ch);
    } else {
      send_to_char("^cAn invisible force is whipping small objects around the area.^n\r\n", ch);
    }
  }
  if (ch->in_room->icesheet[0] > 0)
    send_to_char("^CIce covers the floor.^n\r\n", ch);
  if (ch->in_room->silence[0] > 0)
    send_to_char("^LAn unnatural hush stills the air.^n\r\n", ch);

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
      send_to_char(ch, "%s...There are vehicles for sale here.^n\r\n", ch->in_room->vehicles ? "^Y" : "^y");
    }

    // TODO: Add any other relevant funcs with room displays here.
  }

  // Show a training info string for totalinvis trainers.
  for (struct char_data *trainer = ch->in_room->people; trainer; trainer = trainer->next_in_room) {
    if (MOB_HAS_SPEC(trainer, teacher) && MOB_FLAGGED(trainer, MOB_TOTALINVIS)) {
      send_to_char(ch, "%s...You can practice skills here.^n\r\n", ch->in_room->vehicles ? "^Y" : "^y");
      break;
    }
  }

  /* now list characters & objects */
  // what fun just to get a colorized listing
  CCHAR = "^g";
  CGLOB = KGRN;
  // Show non-corpses.
  list_obj_to_char(ch->in_room->contents, ch, SHOW_MODE_ON_GROUND, FALSE, FALSE);
  CGLOB = KNRM;
  CCHAR = NULL;
  // Show corpses.
  list_obj_to_char(ch->in_room->contents, ch, SHOW_MODE_ON_GROUND, FALSE, TRUE);
  list_char_to_char(ch->in_room->people, ch);
  CCHAR = "^y";
  list_veh_to_char(ch->in_room->vehicles, ch);
}

void peek_into_adjacent(struct char_data * ch, int dir)
{
      struct room_data *original_loc = ch->in_room;
      struct room_data *targ_loc = EXIT(ch, dir)->to_room;

      char_from_room(ch);
      char_to_room(ch, targ_loc);
      look_at_room(ch, 1, 0);
      char_from_room(ch);
      char_to_room(ch, original_loc);
}

void look_in_direction(struct char_data * ch, int dir)
{
  if (EXIT(ch, dir) && EXIT(ch, dir)->to_room)
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
      send_to_char(ch, "The %s %s destroyed.\r\n", fname(EXIT(ch, dir)->keyword), !strcmp(fname(EXIT(ch, dir)->keyword), "doors") ? "are" : "is");
    else if (IS_SET(EXIT(ch, dir)->exit_info, EX_CLOSED) && EXIT(ch, dir)->keyword)
      send_to_char(ch, "The %s %s closed.\r\n", fname(EXIT(ch, dir)->keyword), !strcmp(fname(EXIT(ch, dir)->keyword), "doors") ? "are" : "is");
    else if (IS_SET(EXIT(ch, dir)->exit_info, EX_ISDOOR) && EXIT(ch, dir)->keyword)
      send_to_char(ch, "The %s %s open.\r\n", fname(EXIT(ch, dir)->keyword), !strcmp(fname(EXIT(ch, dir)->keyword), "doors") ? "are" : "is");

    if (GET_APARTMENT(ch->in_room)){
      /* Apartments have peepholes. */
      send_to_char("Through the peephole, you see:\r\n", ch);
      peek_into_adjacent(ch, dir);
    }
    else if (IS_SET(EXIT(ch, dir)->exit_info, EX_WINDOWED)){
      /* You can see through windows. */
      send_to_char("Through the window, you see:\r\n", ch);
      peek_into_adjacent(ch, dir);
    }
    else if (IS_SET(EXIT(ch, dir)->exit_info, EX_BARRED_WINDOW)){
      /* You can see through bars as well. */
      send_to_char("Through the bars, you see:\r\n", ch);
      peek_into_adjacent(ch, dir);
    }

  } else
    send_to_char("You see nothing special.\r\n", ch);
}

void _send_obj_contents_info_to_char(struct obj_data *obj, struct char_data *ch, int bits) {
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
  if (obj->contains) {
    list_obj_to_char(obj->contains, ch, SHOW_MODE_INSIDE_CONTAINER, TRUE, FALSE);
  } else {
    send_to_char("  Nothing.\r\n", ch);
  }
}

void look_in_obj(struct char_data * ch, struct obj_data *obj, bool exa, int bits) {
  if (!obj) {
    send_to_char("Look in what?\r\n", ch);
    return;
  }

  switch (GET_OBJ_TYPE(obj)) {
    case ITEM_GUN_AMMO:
      send_to_char(ch, "It contains %d %s.\r\n",
                   GET_AMMOBOX_QUANTITY(obj),
                   get_ammo_representation(GET_AMMOBOX_WEAPON(obj), GET_AMMOBOX_TYPE(obj), GET_AMMOBOX_QUANTITY(obj), ch));
      break;
    case ITEM_WORN:
    case ITEM_SHOPCONTAINER:
      _send_obj_contents_info_to_char(obj, ch, bits);
      break;
    case ITEM_CONTAINER:
    case ITEM_HOLSTER:
    case ITEM_QUIVER:
    case ITEM_KEYRING:
      if (IS_SET(GET_CONTAINER_FLAGS(obj), CONT_CLOSED)) {
        send_to_char("It is closed.\r\n", ch);
      } else {
        if (!exa) {
          _send_obj_contents_info_to_char(obj, ch, bits);
        } else {
          list_obj_to_char(obj->contains, ch, SHOW_MODE_INSIDE_CONTAINER, TRUE, FALSE);
        }
      }
      break;
    case ITEM_FOUNTAIN:
    case ITEM_DRINKCON:
      if (GET_DRINKCON_AMOUNT(obj) <= 0) {
        send_to_char("It is empty.\r\n", ch);
      } else {
        float full_ratio = GET_DRINKCON_AMOUNT(obj) / (MAX(1, GET_DRINKCON_MAX_AMOUNT(obj)));
        int amt = MIN(3, MAX(0, 3 * full_ratio));
        snprintf(buf, sizeof(buf), "It's %sfull of a %s liquid.\r\n", fullness[amt],
                color_liquid[GET_DRINKCON_LIQ_TYPE(obj)]);
        send_to_char(buf, ch);
      }
      break;
    default:
      send_to_char(ch, "There's nothing inside %s!\r\n", GET_OBJ_NAME(obj));
      break;
  }
}

void look_in_obj(struct char_data * ch, char *arg, bool exa)
{
  struct char_data *fake_ch_for_rigging = ch;
  struct obj_data *obj = NULL;
  struct char_data *dummy = NULL;
  struct veh_data *veh = NULL;
  int bits;

  if (!*arg) {
    send_to_char("Look in what?\r\n", ch);
    return;
  }

  if (IS_RIGGING(ch)) {
    struct veh_data *rigged = NULL;
    RIG_VEH(ch, rigged);

    if (rigged) {
      fake_ch_for_rigging = read_mobile(3, VIRTUAL);

      if (rigged->in_room)
        char_to_room(fake_ch_for_rigging, rigged->in_room);
      else if (rigged->in_veh) {
        char_to_veh(rigged->in_veh, fake_ch_for_rigging);
        fake_ch_for_rigging->vfront = FALSE;
      }
      else {
        send_to_char(ch, "Your vehicle is being towed.\r\n");
        return;
      }
    }
  }

  // Find the specified thing. Vehicle will take priority over object.
  bits = generic_find(arg, FIND_OBJ_INV | FIND_OBJ_ROOM | FIND_OBJ_EQUIP, fake_ch_for_rigging, &dummy, &obj);
  if (fake_ch_for_rigging->in_veh) {
    veh = get_veh_list(arg, fake_ch_for_rigging->in_veh->carriedvehs, ch);
  } else {
    veh = get_veh_list(arg, fake_ch_for_rigging->in_room->vehicles, ch);
  }

  if (!bits && !veh) {
    if (str_str("pockets", arg)) {
      send_to_char("Please see ^WHELP POCKETS^n for info on how to use your ammo pockets.\r\n", ch);
    } else {
      send_to_char(ch, "There doesn't seem to be %s %s here.\r\n", AN(arg), arg);
    }
    if (fake_ch_for_rigging && fake_ch_for_rigging != ch)
      extract_char(fake_ch_for_rigging);
    return;
  }

  if (veh) {
    struct veh_data *curr_in_veh = ch->in_veh;
    bool curr_vfront = ch->vfront;

    /* Disabled-- a check with no penalty that you can spam until success does not add to the fun.
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
    */
    // Notify the occupants, update character to be an occupant, look in the vehicle, then reset character's occupancy.
    for (struct char_data *vict = veh->people; vict; vict = vict->next_in_veh)
      act("$n peers inside.", FALSE, ch, 0, vict, TO_VICT);
    ch->in_veh = veh;
    ch->vfront = TRUE;
    look_in_veh(ch);
    ch->in_veh = curr_in_veh;
    ch->vfront = curr_vfront;

    if (fake_ch_for_rigging && fake_ch_for_rigging != ch)
      extract_char(fake_ch_for_rigging);
    return;
  } else {
    look_in_obj(ch, obj, exa, bits);
  }

  if (fake_ch_for_rigging && fake_ch_for_rigging != ch)
    extract_char(fake_ch_for_rigging);
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
void look_at_target(struct char_data * ch, char *arg, char *extra_args)
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
  } else if (ch->in_veh) {
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
    // If this isn't an exdesc invocation, look at the character.
    if (!look_at_exdescs(ch, found_char, extra_args)) {
      look_at_char(found_char, ch, arg);
    }
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
      // If this isn't an exdesc invocation, look at the character.
      if (!look_at_exdescs(ch, found_char, arg)) {
        look_at_char(found_char, ch, arg);
      }
      /*
      if (ch != found_char) {
        if (CAN_SEE(found_char, ch)) {
          send_to_char(found_char, "%s looks at you.\r\n", GET_NAME(ch));
        }
        snprintf(buf, sizeof(buf), "%s looks at %s.\r\n", GET_NAME(ch), GET_NAME(found_char));
        send_to_veh(buf, ch->in_veh, ch, found_char, FALSE);
      }
      */
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
      show_obj_to_char(found_obj, ch, SHOW_MODE_JUST_DESCRIPTION);       /* Show no-description */
    else
      show_obj_to_char(found_obj, ch, SHOW_MODE_JUST_SHOW_GLOW_ETC);       /* Find hum, glow etc */
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
  int look_type;

  if (!ch->desc)
    return;

  if (GET_POS(ch) < POS_SLEEPING)
    send_to_char("You can't see anything but stars!\r\n", ch);
  else if (!LIGHT_OK(ch)) {
    send_to_char("It is pitch black...\r\n", ch);
  } else {
    char *remainder = any_one_arg(argument, arg);
    skip_spaces(&remainder);

    if (subcmd == SCMD_READ) {
      if (!*arg)
        send_to_char("Read what?\r\n", ch);
      else {
        skip_spaces(&remainder);
        look_at_target(ch, arg, remainder);
      }
      return;
    }
    if (!*arg)                  /* "look" alone, without an argument at all */
      look_at_room(ch, 1, subcmd == SCMD_QUICKLOOK);
    else if (is_abbrev(arg, "in"))
      look_in_obj(ch, remainder, FALSE);
    /* did the char type 'look <direction>?' */
    else if ((look_type = search_block(arg, lookdirs, FALSE)) >= 0 || (look_type = search_block(arg, fulllookdirs, FALSE)) >= 0)
      look_in_direction(ch, convert_look[look_type]);
    else if (is_abbrev(arg, "at"))
      do_examine(ch, remainder, 0, SCMD_EXAMINE);
    else
      do_examine(ch, argument, 0, SCMD_EXAMINE);
    /* else if (is_abbrev(arg, "at"))
      look_at_target(ch, remainder); // todo split remainder again to get token
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
  remove_vehicle_brain(k);
  repair_vehicle_seating(k);
  snprintf(buf, sizeof(buf), "Name: '^y%s^n', Aliases: %s\r\n",
          k->short_description, k->name);
  snprintf(ENDOF(buf), sizeof(buf) - strlen(buf), "It is a ^c%s^n with handling ^c%d^n, a top speed of ^c%d^n, and raw acceleration of ^c%d^n.\r\n",
          veh_types[k->type], k->handling, k->speed, k->accel);
  snprintf(ENDOF(buf), sizeof(buf) - strlen(buf), "It has a body rating of ^c%d^n and rating-^c%d^n vehicular armor. It seats ^c%d^n up front and ^c%d^n in the back.\r\n",
          k->body, k->armor, k->seating[SEATING_FRONT], k->seating[SEATING_REAR]);
  snprintf(ENDOF(buf), sizeof(buf) - strlen(buf), "Its signature rating is ^c%d^n, and its NERP pilot rating is ^c%d^n.\r\n",
          k->sig, k->pilot);
  snprintf(ENDOF(buf), sizeof(buf) - strlen(buf), "It has a grade ^c%d^n autonav and carrying capacity of ^c%d^n (%d in use).\r\n",
          k->autonav, (int)k->load, (int)k->usedload);
          snprintf(ENDOF(buf), sizeof(buf) - strlen(buf), "Its engine is adapted for ^c%s^n. If loaded into another vehicle, it takes up ^c%d^n load.\r\n",
                  engine_types[k->engine], calculate_vehicle_entry_load(k));
  if (veh_can_traverse_air(k)) {
    strlcat(buf, "It can fly.\r\n", sizeof(buf));
  } else {
    snprintf(ENDOF(buf), sizeof(buf) - strlen(buf), "It can travel over ^c%s%s%s^n.\r\n",
           veh_can_traverse_land(k) ? "land" : "",
           veh_can_traverse_land(k) && veh_can_traverse_water(k) ? " and " : "",
           veh_can_traverse_water(k) ? "water" : "");
  }
  if (k->type == VEH_DRONE) {
    strlcat(buf, "It can't be entered.\r\n", sizeof(buf));
  } else {
    snprintf(ENDOF(buf), sizeof(buf) - strlen(buf), "It ^c%s^n enough space to set up a workshop inside it.\r\n", k->flags.IsSet(VFLAG_WORKSHOP) ? "has" : "doesn't have");
  }
  send_to_char(buf, ch);
}

void do_probe_object(struct char_data * ch, struct obj_data * j, bool is_in_shop) {
  int i, found, mount_location, bal, imp;
  bool has_pockets = FALSE, added_extra_carriage_return = FALSE, has_smartlink = FALSE;
  struct obj_data *accessory = NULL;

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
      strlcat(buf, "It is an ^cinfinite^n light source.", sizeof(buf));
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
      if (IS_GUN(GET_WEAPON_ATTACK_TYPE(j))) {
        int burst_count = 0;

        // Line 1: "It is a 3-round pistol that uses the Pistols skill to fire. Its base damage code is 2L."
        {
          if (GET_WEAPON_MAX_AMMO(j) > 0) {
            snprintf(ENDOF(buf), sizeof(buf) - strlen(buf), "It is a ^c%d-round %s^n that uses the ^c%s^n skill to fire.",
                    GET_WEAPON_MAX_AMMO(j),
                    weapon_types[GET_WEAPON_ATTACK_TYPE(j)],
                    skills[GET_WEAPON_SKILL(j)].name);
          } else {
            snprintf(ENDOF(buf), sizeof(buf) - strlen(buf), "It is %s ^c%s^n that uses the ^c%s^n skill to fire.",
                    AN(weapon_types[GET_WEAPON_ATTACK_TYPE(j)]),
                    weapon_types[GET_WEAPON_ATTACK_TYPE(j)],
                    skills[GET_WEAPON_SKILL(j)].name);
          }
          // Damage code.
          snprintf(ENDOF(buf), sizeof(buf) - strlen(buf), " Its base damage code is ^c%d%s%s^n",
                   GET_WEAPON_POWER(j),
                   wound_arr[GET_WEAPON_DAMAGE_CODE(j)],
                   !IS_DAMTYPE_PHYSICAL(get_weapon_damage_type(j)) ? " (stun)" : "");

          // Burst fire?
          if (GET_WEAPON_FIREMODE(j) == MODE_BF || GET_WEAPON_FIREMODE(j) == MODE_FA) {
            burst_count = GET_WEAPON_FIREMODE(j) == MODE_BF ? 3 : GET_WEAPON_FULL_AUTO_COUNT(j);

            snprintf(ENDOF(buf), sizeof(buf) - strlen(buf), ", raised to ^c%d%s%s^n by its current fire mode.\r\n",
                     GET_WEAPON_POWER(j) + burst_count,
                     wound_arr[MIN(DEADLY, GET_WEAPON_DAMAGE_CODE(j) + (int)(burst_count / 3))],
                     !IS_DAMTYPE_PHYSICAL(get_weapon_damage_type(j)) ? " (stun)" : "");
          } else {
            strlcat(buf, ".\r\n", sizeof(buf));
          }
        }

        // Second line: "It has the following available firemodes: Semi-Automatic"
        {
          strlcat(buf, "It has the following available firemodes:", sizeof(buf));
          bool first_mode = TRUE;
          for (int mode = MODE_SS; mode <= MODE_FA; mode++)
            if (WEAPON_CAN_USE_FIREMODE(j, mode)) {
              snprintf(ENDOF(buf), sizeof(buf) - strlen(buf), "%s ^c%s^n", first_mode ? "" : ",", fire_mode[mode]);
              first_mode = FALSE;
            }
          if (!first_mode && PRF_FLAGGED(ch, PRF_SEE_TIPS))
            strlcat(buf, "  (see ##^WHELP MODE^n)", sizeof(buf));
          strlcat(buf, "\r\n", sizeof(buf));
        }

        // Third line (optional): "It is loaded with EX, which increases power by two."
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
            case AMMO_AV:
              strlcat(buf, "pierces through enemy ballistic and vehicle armor, halving its value.", sizeof(buf));
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
              strlcat(buf, "doubles knockdown effectiveness, but deals mental instead of physical damage, and treats the enemy's impact armor as two points higher.", sizeof(buf));
              break;
          }
        }

        // Fourth line (optional): Integral recoil compensation or per-shot recoil add.
        {
          if (GET_WEAPON_INTEGRAL_RECOIL_COMP(j) > 0) {
            snprintf(ENDOF(buf), sizeof(buf) - strlen(buf), "\r\nIt has ^c%d^n point%s of integral recoil compensation.",
                    GET_WEAPON_INTEGRAL_RECOIL_COMP(j),
                    GET_WEAPON_INTEGRAL_RECOIL_COMP(j) != 1 ? "s" : "");
          }
          else if (GET_WEAPON_INTEGRAL_RECOIL_COMP(j) < 0) {
            snprintf(ENDOF(buf), sizeof(buf) - strlen(buf), "\r\nIt adds ^c%d^n point%s of recoil per shot.",
                    -GET_WEAPON_INTEGRAL_RECOIL_COMP(j),
                    -GET_WEAPON_INTEGRAL_RECOIL_COMP(j) != 1 ? "s" : "");
          }
        }

        // Info about attachments, if any.
        int standing_recoil_comp = GET_WEAPON_INTEGRAL_RECOIL_COMP(j);
        int prone_recoil_comp = 0;
        int real_obj;
        bool has_laser_sight_already = FALSE, has_bayonet = FALSE;
        for (int i = ACCESS_LOCATION_TOP; i <= ACCESS_LOCATION_UNDER; i++) {
          if (GET_WEAPON_ATTACH_LOC(j, i) > 0
              && (real_obj = real_object(GET_WEAPON_ATTACH_LOC(j, i))) > 0
              && (accessory = &obj_proto[real_obj])) {
            // mount_location: used for gun_accessory_locations[] lookup.
            mount_location = i - ACCESS_LOCATION_TOP;

            // format flair
            if (!added_extra_carriage_return) {
              strlcat(buf, "\r\n", sizeof(buf));
              added_extra_carriage_return = TRUE;
            }

            // parse and add the string for the accessory's special bonuses
            switch (GET_ACCESSORY_TYPE(accessory)) {
              case ACCESS_SMARTLINK:
                if (has_smartlink) {
                  strlcat(buf, "^Y\r\nIt has multiple smartlinks attached, and they do not stack. You should remove one and replace it with something else.^n", sizeof(buf));
                } else {
                  has_smartlink = TRUE;

                  int cyberware_rating = 0;
                  for (struct obj_data *cyber = ch->cyberware; cyber; cyber = cyber->next_content) {
                    if (GET_CYBERWARE_TYPE(cyber) == CYB_SMARTLINK) {
                      cyberware_rating = GET_CYBERWARE_RATING(cyber);
                      break;
                    }
                  }

                  // No cyberware?
                  if (cyberware_rating == 0) {
                    // Do they have goggles?
                    if (get_smartgoggle(ch)) {
                      // Goggles limit you to 1/2 of the SL-1 rating.
                      snprintf(ENDOF(buf), sizeof(buf) - strlen(buf), "\r\nThe Smartlink%s attached to the %s ^yis limited by your use of goggles^n and only provides ^c-1^n to attack difficulty (aka TN; lower is better).",
                               GET_ACCESSORY_RATING(accessory) == 2 ? "-II" : "",
                               gun_accessory_locations[mount_location]);
                    }
                    // No goggles either.
                    else {
                      snprintf(ENDOF(buf), sizeof(buf) - strlen(buf), "\r\n^YIt has a Smartlink%s attached to the %s, but you have neither the cyberware nor goggles to use it.^n",
                               GET_ACCESSORY_RATING(accessory) == 2 ? "-II" : "",
                               gun_accessory_locations[mount_location]);
                      // You don't get to use it: unflag this weapon as having a smartlink.
                      has_smartlink = FALSE;
                    }
                  }

                  // cyberware-1
                  else if (cyberware_rating == 1) {
                    if (GET_ACCESSORY_RATING(accessory) == 1) {
                      // SL-1 and cyberware-1? Just fine.
                      snprintf(ENDOF(buf), sizeof(buf) - strlen(buf), "\r\nThe Smartlink attached to the %s provides ^c%d^n to attack difficulty (aka TN; lower is better).",
                               gun_accessory_locations[mount_location],
                               -SMARTLINK_I_MODIFIER);
                    } else {
                      // SL-2 and cyberware-1? Limited.
                      snprintf(ENDOF(buf), sizeof(buf) - strlen(buf), "\r\nThe Smartlink-II attached to the %s ^yis limited by your cyberware^n and only provides ^c%d^n to attack difficulty (aka TN; lower is better).",
                               gun_accessory_locations[mount_location],
                               -SMARTLINK_I_MODIFIER);
                    }
                  }

                  // cyberware-2+
                  else {
                    snprintf(ENDOF(buf), sizeof(buf) - strlen(buf), "\r\nThe Smartlink%s attached to the %s provides ^c%d^n to attack difficulty (aka TN; lower is better).",
                            GET_ACCESSORY_RATING(accessory) < 2 ? "" : "-II", gun_accessory_locations[mount_location],
                            (GET_ACCESSORY_RATING(accessory) == 1 || GET_ACCESSORY_RATING(accessory) < 2) ? -SMARTLINK_I_MODIFIER : -SMARTLINK_II_MODIFIER);
                  }
                }
                break;
              case ACCESS_SCOPE:
                if (GET_OBJ_AFFECT(accessory).IsSet(AFF_LASER_SIGHT)) {
                  if (has_laser_sight_already) {
                    strlcat(buf, "^Y\r\nIt has multiple laser sights attached, and they do not stack. You should remove one and replace it with something else.^n", sizeof(buf));
                  } else {
                    snprintf(ENDOF(buf), sizeof(buf) - strlen(buf), "\r\nThe laser sight attached to the %s provides ^c-1^n to attack difficulty (aka TN; lower is better).",
                            gun_accessory_locations[mount_location]);
                  }
                  has_laser_sight_already = TRUE;
                } else {
                  snprintf(ENDOF(buf), sizeof(buf) - strlen(buf), "\r\nA scope has been attached to the %s.",
                          gun_accessory_locations[mount_location]);
                }
                break;
              case ACCESS_GASVENT:
                snprintf(ENDOF(buf), sizeof(buf) - strlen(buf), "\r\nThe gas vent installed in the %s provides ^c%d^n round%s worth of recoil compensation.",
                        gun_accessory_locations[mount_location], -GET_ACCESSORY_RATING(accessory), GET_ACCESSORY_RATING(accessory) > 1 ? "s'" : "'s");
                standing_recoil_comp -= GET_ACCESSORY_RATING(accessory);
                break;
              case ACCESS_SHOCKPAD:
                snprintf(ENDOF(buf), sizeof(buf) - strlen(buf), "\r\nThe attachment installed on the %s provides ^c1^n round's worth of recoil compensation.",
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
                snprintf(ENDOF(buf), sizeof(buf) - strlen(buf), "\r\nThe bipod installed on the %s provides ^c%d^n round%s worth of recoil compensation when fired from prone.",
                        gun_accessory_locations[mount_location], RECOIL_COMP_VALUE_BIPOD, RECOIL_COMP_VALUE_BIPOD > 1 ? "s'" : "'s");
                prone_recoil_comp += RECOIL_COMP_VALUE_BIPOD;
                break;
              case ACCESS_TRIPOD:
                snprintf(ENDOF(buf), sizeof(buf) - strlen(buf), "\r\nThe tripod installed on the %s provides ^c%d^n round%s worth of recoil compensation when fired from prone.",
                        gun_accessory_locations[mount_location], RECOIL_COMP_VALUE_TRIPOD, RECOIL_COMP_VALUE_TRIPOD > 1 ? "s'" : "'s");
                prone_recoil_comp += RECOIL_COMP_VALUE_TRIPOD;
                break;
              case ACCESS_BAYONET:
                if (mount_location != ACCESS_ACCESSORY_LOCATION_UNDER) {
                  strlcat(buf, "\r\n^RThe bayonet has been mounted in the wrong location and is nonfunctional. Alert an imm.", sizeof(buf));
                } else {
                  snprintf(ENDOF(buf), sizeof(buf) - strlen(buf), "\r\nThe bayonet attached to the %s allows you to use the Pole Arms skill when defending from melee attacks. It also lets you ^WEJECT^n your magazine to do a bayonet charge.",
                          gun_accessory_locations[mount_location]);
                  has_bayonet = TRUE;
                }
                break;
              default:
                snprintf(buf1, sizeof(buf1), "SYSERR: Unknown accessory type %d passed to do_probe_object()", GET_ACCESSORY_TYPE(accessory));
                log(buf1);
                break;
            }
            // Tack on affect and extra flags to the attachment.
            if (strcmp(GET_OBJ_AFFECT(accessory).ToString(), "0") != 0) {
              GET_OBJ_AFFECT(accessory).PrintBits(buf2, MAX_STRING_LENGTH, affected_bits, AFF_MAX);
              snprintf(ENDOF(buf), sizeof(buf) - strlen(buf), "\r\n ^- It provides the following flags: ^c%s^n", buf2);
            }

            if (strcmp(GET_OBJ_EXTRA(accessory).ToString(), "0") != 0) {
              GET_OBJ_EXTRA(accessory).PrintBits(buf2, MAX_STRING_LENGTH, pc_readable_extra_bits, MAX_ITEM_EXTRA);
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

        // Info on how it strikes in melee.
        snprintf(ENDOF(buf), sizeof(buf) - strlen(buf), "\r\nIf used in melee clashes, it is a ^c(STR)M %s^n with ^c%d^n reach.",
                 has_bayonet ? "polearm" : "club",
                 (GET_WEAPON_SKILL(j) == SKILL_PISTOLS ? 0 : 1) + (has_bayonet ? 1 : 0));

        // Heavy weapon? Warn the wielder.
        if (GET_WEAPON_SKILL(j) >= SKILL_MACHINE_GUNS && GET_WEAPON_SKILL(j) <= SKILL_ASSAULT_CANNON) {
          if (GET_BOD(ch) < 8 || GET_STR(ch) < 8) {
            strlcat(buf, "\r\n^YYou will be unable to wield it without using a gyromount or going prone.^n", sizeof(buf));
          }
          strlcat(buf, "\r\n^yAs a heavy weapon, it has the potential to damage you when fired.^n", sizeof(buf));
        }

      }
      // Grenades second.
      else if (GET_WEAPON_ATTACK_TYPE(j) == WEAP_GRENADE) {
        switch (GET_WEAPON_GRENADE_TYPE(j)) {
          case GRENADE_TYPE_FLASHBANG:
            snprintf(ENDOF(buf), sizeof(buf) - strlen(buf), "It is a ^Wflashbang grenade^n that uses the ^c%s^n skill to throw.\r\n",
                     skills[SKILL_THROWING_WEAPONS].name);
            snprintf(ENDOF(buf), sizeof(buf) - strlen(buf), "Magically-active victims must roll against TN ^c%d^n to avoid spell degradation when hit by its blast.\r\n",
                     GET_WEAPON_POWER(j));
            strlcat(buf, "\r\nIt is ^ynon-canon^n and cannot be used in tabletop runs.", sizeof(buf));
            break;
          case GRENADE_TYPE_EXPLOSIVE:
            snprintf(ENDOF(buf), sizeof(buf) - strlen(buf), "It is an ^Wexplosive grenade^n that uses the ^c%s^n skill to attack with. It has not been fully implemented.",
                     skills[SKILL_THROWING_WEAPONS].name);
            break;
          default:
            mudlog_vfprintf(ch, LOG_SYSLOG, "SYSERR: Unknown grenade type %d in probe.", GET_WEAPON_GRENADE_TYPE(j));
            strlcat(buf, "It's a BUGGED grenade. Alert staff!", sizeof(buf));
            break;
        }
      }
      // Melee weapons.
      else {
        if (obj_index[GET_OBJ_RNUM(j)].wfunc == monowhip) {
          snprintf(ENDOF(buf), sizeof(buf) - strlen(buf), "It is a ^cmonowhip^n that uses the ^c%s^n skill to attack with. Its damage code is ^c10%s%s^n.",
                  skills[GET_OBJ_VAL(j, 4)].name,
                  wound_arr[GET_OBJ_VAL(j, 1)],
                  !IS_DAMTYPE_PHYSICAL(get_weapon_damage_type(j)) ? " (stun)" : "");
        }
        else if (GET_WEAPON_STR_BONUS(j) != 0) {
          snprintf(ENDOF(buf), sizeof(buf) - strlen(buf), "It is a ^c%s^n that uses the ^c%s^n skill to attack with. Its damage code is ^c(STR%s%d)%s%s^n.",
                   GET_WEAPON_TYPE_NAME(GET_WEAPON_ATTACK_TYPE(j)),
                   skills[GET_WEAPON_SKILL(j)].name,
                   GET_WEAPON_STR_BONUS(j) < 0 ? "" : "+",
                   GET_WEAPON_STR_BONUS(j),
                   GET_WOUND_NAME(GET_WEAPON_DAMAGE_CODE(j)),
                   !IS_DAMTYPE_PHYSICAL(get_weapon_damage_type(j)) ? " (stun)" : "");
        } else {
          snprintf(ENDOF(buf), sizeof(buf) - strlen(buf), "It is a ^c%s^n that uses the ^c%s^n skill to attack with. Its damage code is ^c(STR)%s%s^n.",
                   GET_WEAPON_TYPE_NAME(GET_WEAPON_ATTACK_TYPE(j)), 
                   skills[GET_WEAPON_SKILL(j)].name,
                   GET_WOUND_NAME(GET_WEAPON_DAMAGE_CODE(j)),
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

      if (IS_OBJ_STAT(j, ITEM_EXTRA_HARDENED_ARMOR)) {
        if (is_in_shop) {
          strlcat(buf, "It is customized armor that will be fitted permanently to the first person to wear it.\r\n", sizeof(buf));
        } else {
          if (GET_WORN_HARDENED_ARMOR_CUSTOMIZED_FOR(j) == GET_IDNUM(ch)) {
            strlcat(buf, "It has been permanently customized to fit you. Nobody else can wear it.\r\n", sizeof(buf));
          } else if (GET_WORN_HARDENED_ARMOR_CUSTOMIZED_FOR(j) == HARDENED_ARMOR_NOT_CUSTOMIZED) {
            strlcat(buf, "^gIt will be permanently customized to fit the first person to wear it (AKA soul-bound).^n\r\n", sizeof(buf));
          } else {
            strlcat(buf, "^yIt has been permanently customized to fit someone else-- you can't wear it.^n\r\n", sizeof(buf));
          }
        }
      }
      break;
    case ITEM_DOCWAGON:
      snprintf(ENDOF(buf), sizeof(buf) - strlen(buf), "It is a ^c%s^n contract that ^c%s bonded%s^n.",
              docwagon_contract_types[GET_DOCWAGON_CONTRACT_GRADE(j)],
              GET_DOCWAGON_BONDED_IDNUM(j) ? "is" : "has not been",
              GET_DOCWAGON_BONDED_IDNUM(j) ? (GET_DOCWAGON_BONDED_IDNUM(j) == GET_IDNUM(ch) ? " to you" : " to someone else") : " to anyone yet^n, and ^ywill not function until it is^n");

      if (GET_DOCWAGON_BONDED_IDNUM(j) == GET_IDNUM(ch) && !j->worn_by) {
        strlcat(buf, "\r\n\r\n^yIt must be worn to be effective.^n", sizeof(buf));
      }
      break;
    case ITEM_CONTAINER:
      snprintf(ENDOF(buf), sizeof(buf) - strlen(buf), "It can hold a maximum of ^c%d^n kilograms.", GET_OBJ_VAL(j, 0));
      break;
    case ITEM_DRINKCON:
      sprinttype(GET_DRINKCON_LIQ_TYPE(j), drinks, buf2, sizeof(buf2));
      snprintf(ENDOF(buf), sizeof(buf) - strlen(buf), "It currently contains ^c%d/%d^n units of ^c%s^n.",
               GET_DRINKCON_AMOUNT(j), GET_DRINKCON_MAX_AMOUNT(j), buf2);
      break;
    case ITEM_FOUNTAIN:
      sprinttype(GET_FOUNTAIN_LIQ_TYPE(j), drinks, buf2, sizeof(buf2));
      snprintf(ENDOF(buf), sizeof(buf) - strlen(buf), "It currently contains ^c%d/%d^n units of ^c%s^n.",
               GET_FOUNTAIN_AMOUNT(j), GET_FOUNTAIN_MAX_AMOUNT(j), buf2);
      break;
    case ITEM_MONEY:
      if (!GET_ITEM_MONEY_IS_CREDSTICK(j))
        snprintf(ENDOF(buf), sizeof(buf) - strlen(buf), "It is a stack of cash worth ^c%d^n nuyen.", GET_ITEM_MONEY_VALUE(j));
      else {
        if (GET_ITEM_MONEY_CREDSTICK_IS_PC_OWNED(j)) {
          if (!IS_NPC(ch) && GET_ITEM_MONEY_CREDSTICK_OWNER_ID(j) == GET_IDNUM(ch)) {
            snprintf(ENDOF(buf), sizeof(buf) - strlen(buf), "It is a ^c%s^n-secured ^ccredstick^n linked to your bank account, which has ^c%ld^n nuyen.",
                    (GET_OBJ_VAL(j, 2) == 1 ? "6-digit PIN" : (GET_OBJ_VAL(j, 2) == 2 ? "thumbprint" : "retina")), GET_BANK(ch));
          } else {
            snprintf(ENDOF(buf), sizeof(buf) - strlen(buf), "It is a ^c%s^n-secured ^ccredstick^n linked to someone else's bank account.",
                    (GET_OBJ_VAL(j, 2) == 1 ? "6-digit PIN" : (GET_OBJ_VAL(j, 2) == 2 ? "thumbprint" : "retina")));
          }
        } else {
          snprintf(ENDOF(buf), sizeof(buf) - strlen(buf), "It is %s ^c%s^n-secured ^ccredstick^n loaded with ^c%d^n nuyen.",
                    GET_ITEM_MONEY_CREDSTICK_OWNER_ID(j) ? "a" : "an unlocked",
                    (GET_OBJ_VAL(j, 2) == 1 ? "6-digit PIN" : (GET_OBJ_VAL(j, 2) == 2 ? "thumbprint" : "retina")), 
                    GET_ITEM_MONEY_VALUE(j));
        }
      }
      break;
    case ITEM_KEY:
      strlcat(buf, "No OOC information is available about this key.", sizeof(buf));
      break;
    case ITEM_FOOD:
#ifdef ENABLE_HUNGER
      snprintf(ENDOF(buf), sizeof(buf) - strlen(buf), "It provides ^c%d^n units of nutrition when eaten.", GET_OBJ_VAL(j, 0));
#endif
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
      snprintf(ENDOF(buf), sizeof(buf) - strlen(buf), "It is a ^crating-%d %s^n patch", GET_PATCH_RATING(j), patch_names[GET_PATCH_TYPE(j)]);
      switch (GET_PATCH_TYPE(j)) {
        case PATCH_STIM:
          strlcat(buf, ", which restores mental damage but can only be used by mundanes.", sizeof(buf));
          break;
        case PATCH_TRANQ:
          strlcat(buf, ", which causes mental damage.", sizeof(buf));
          break;
        case PATCH_TRAUMA:
          strlcat(buf, ", which can stabilize a mortally-wounded patient.", sizeof(buf));
          break;
        case PATCH_ANTIDOTE:
          strlcat(buf, ", which lessens the effect of drugs and toxins. (not yet implemented)", sizeof(buf));
          break;
      }
      break;
    case ITEM_CYBERDECK:
      snprintf(ENDOF(buf), sizeof(buf) - strlen(buf), "MPCP: ^c%d^n, Hardening: ^c%d^n, Active: ^c%d^n, Storage: ^c%d^n, Load: ^c%d^n.",
              GET_OBJ_VAL(j, 0), GET_OBJ_VAL(j, 1), GET_OBJ_VAL(j, 2),
              GET_OBJ_VAL(j, 3), GET_OBJ_VAL(j, 4));
      break;
    case ITEM_PROGRAM:
      snprintf(ENDOF(buf), sizeof(buf) - strlen(buf), "It is a ^crating-%d %s^n program, ^c%d^n units in size.",
               GET_PROGRAM_RATING(j), programs[GET_PROGRAM_TYPE(j)].name, GET_PROGRAM_SIZE(j));
      if (GET_PROGRAM_TYPE(j) == SOFT_ATTACK)
        snprintf(ENDOF(buf), sizeof(buf) - strlen(buf), " Its damage code is ^c%s^n.", GET_WOUND_NAME(GET_OBJ_VAL(j, 3)));

      if (GET_OBJ_VNUM(j) == OBJ_BLANK_PROGRAM && (GET_PROGRAM_TYPE(j) >= SOFT_ASIST_COLD || GET_PROGRAM_TYPE(j) < SOFT_SENSOR)) {
        if (GET_OBJ_TIMER(j) < 0)
          strlcat(buf, " It was ruined in cooking and is useless.\r\n", sizeof(buf));
        else if (!GET_OBJ_TIMER(j))
          strlcat(buf, " It still needs to be cooked to a chip.\r\n", sizeof(buf));
        else
          strlcat(buf, " It has been cooked and is ready for use.\r\n", sizeof(buf));
      }
      break;
    case ITEM_BIOWARE:
      if (GET_BIOWARE_RATING(j) > 0) {
        snprintf(ENDOF(buf), sizeof(buf) - strlen(buf), "It is a ^crating-%d %s%s^n that uses ^c%.2f^n index when installed.",
                GET_BIOWARE_RATING(j), GET_BIOWARE_IS_CULTURED(j) ? "cultured " : "",
                decap_bio_types[GET_BIOWARE_TYPE(j)], ((float) calculate_ware_essence_or_index_cost(ch, j) / 100));
      } else {
        snprintf(ENDOF(buf), sizeof(buf) - strlen(buf), "It is a ^c%s%s^n that uses ^c%.2f^n index when installed.",
                GET_BIOWARE_IS_CULTURED(j) ? "cultured " : "",
                decap_bio_types[GET_BIOWARE_TYPE(j)], ((float) calculate_ware_essence_or_index_cost(ch, j) / 100));
      }
      break;
    case ITEM_CYBERWARE:
      strlcat(buf, "It is a ^c", sizeof(buf));

      if (GET_CYBERWARE_RATING(j) > 0) {
         snprintf(ENDOF(buf), sizeof(buf) - strlen(buf), "rating-%d ", GET_CYBERWARE_RATING(j));
      }

      char flag_parse[1000];
      *flag_parse = '\0';

      switch (GET_CYBERWARE_TYPE(j)) {
        case CYB_DATAJACK:
          if (GET_CYBERWARE_FLAGS(j) != DATA_STANDARD)
            strlcpy(flag_parse, " induction", sizeof(flag_parse));
          else
            strlcpy(flag_parse, " standard", sizeof(flag_parse));
          break;
        case CYB_TOOTHCOMPARTMENT:
          if (GET_CYBERWARE_FLAGS(j))
            strlcpy(flag_parse, " breakable", sizeof(flag_parse));
          else
            strlcpy(flag_parse, " storage", sizeof(flag_parse));
          break;
        case CYB_HANDSPUR:
        case CYB_HANDBLADE:
        case CYB_FANGS:
        case CYB_HORNS:
          if (GET_CYBERWARE_FLAGS(j))
            strlcpy(flag_parse, " retractable", sizeof(flag_parse));
          break;
        case CYB_HANDRAZOR:
          if (IS_SET(GET_CYBERWARE_FLAGS(j), 1 << CYBERWEAPON_RETRACTABLE))
            strlcpy(flag_parse, " retractable", sizeof(flag_parse));

          if (IS_SET(GET_CYBERWARE_FLAGS(j), 1 << CYBERWEAPON_IMPROVED)) {
            if (*flag_parse) {
              strlcat(flag_parse, ", improved", sizeof(flag_parse));
            } else {
              strlcat(flag_parse, " improved", sizeof(flag_parse));
            }
          }
          break;
        case CYB_BONELACING:
          snprintf(flag_parse, sizeof(flag_parse), " %s", bone_lacing[GET_CYBERWARE_FLAGS(j)]);
          break;
        case CYB_REFLEXTRIGGER:
          if (GET_CYBERWARE_FLAGS(j))
            strlcpy(flag_parse, " stepped", sizeof(flag_parse));
          break;
        case CYB_SKULL:
          if (IS_SET(GET_CYBERWARE_FLAGS(j), SKULL_MOD_OBVIOUS))
            strlcat(flag_parse, " obvious", sizeof(flag_parse));
          else
            strlcpy(flag_parse, " synthetic", sizeof(flag_parse));

          if (IS_SET(GET_CYBERWARE_FLAGS(j), SKULL_MOD_TAC_COMP))
            strlcpy(flag_parse, ", combat-enhancing", sizeof(flag_parse));

          /*
          if (IS_SET(GET_CYBERWARE_FLAGS(j), SKULL_MOD_ARMOR_MOD1))
            strlcpy(flag_parse, ", armored", sizeof(flag_parse));
          */
          break;
        case CYB_TORSO:
          if (IS_SET(GET_CYBERWARE_FLAGS(j), TORSO_MOD_OBVIOUS))
            strlcat(flag_parse, " obvious", sizeof(flag_parse));
          else
            strlcpy(flag_parse, " synthetic", sizeof(flag_parse));

          /*
          if (IS_SET(GET_CYBERWARE_FLAGS(j), TORSO_MOD_ARMOR_MOD1))
            strlcpy(flag_parse, ", armored (grade 1)", sizeof(flag_parse));
          else if (IS_SET(GET_CYBERWARE_FLAGS(j), TORSO_MOD_ARMOR_MOD2))
            strlcpy(flag_parse, ", armored (grade 2)", sizeof(flag_parse));
          else if (IS_SET(GET_CYBERWARE_FLAGS(j), TORSO_MOD_ARMOR_MOD3))
            strlcpy(flag_parse, ", armored (grade 3)", sizeof(flag_parse));
          */
          break;
        case CYB_LEGS:
          if (IS_SET(GET_CYBERWARE_FLAGS(j), LEGS_MOD_OBVIOUS))
            strlcat(flag_parse, " obvious", sizeof(flag_parse));
          else
            strlcpy(flag_parse, " synthetic", sizeof(flag_parse));

          /*
          if (IS_SET(GET_CYBERWARE_FLAGS(j), LEGS_MOD_ARMOR_MOD1))
            strlcpy(flag_parse, ", armored (grade 1)", sizeof(flag_parse));

          if (IS_SET(GET_CYBERWARE_FLAGS(j), LEGS_MOD_STRENGTH_MOD1))
            strlcpy(flag_parse, ", strengthening (grade 1)", sizeof(flag_parse));
          else if (IS_SET(GET_CYBERWARE_FLAGS(j), LEGS_MOD_STRENGTH_MOD2))
            strlcpy(flag_parse, ", strengthening (grade 2)", sizeof(flag_parse));
          else if (IS_SET(GET_CYBERWARE_FLAGS(j), LEGS_MOD_STRENGTH_MOD3))
            strlcpy(flag_parse, ", strengthening (grade 3)", sizeof(flag_parse));

          if (IS_SET(GET_CYBERWARE_FLAGS(j), LEGS_MOD_QUICKNESS_MOD1))
            strlcpy(flag_parse, ", quickening (grade 1)", sizeof(flag_parse));
          else if (IS_SET(GET_CYBERWARE_FLAGS(j), LEGS_MOD_QUICKNESS_MOD2))
            strlcpy(flag_parse, ", quickening (grade 2)", sizeof(flag_parse));
          else if (IS_SET(GET_CYBERWARE_FLAGS(j), LEGS_MOD_QUICKNESS_MOD3))
            strlcpy(flag_parse, ", quickening (grade 3)", sizeof(flag_parse));
          */
          break;
        case CYB_ARMS:
          if (IS_SET(GET_CYBERWARE_FLAGS(j), ARMS_MOD_OBVIOUS))
            strlcat(flag_parse, " obvious", sizeof(flag_parse));
          else
            strlcpy(flag_parse, " synthetic", sizeof(flag_parse));

          /*
          if (IS_SET(GET_CYBERWARE_FLAGS(j), ARMS_MOD_ARMOR_MOD1))
            strlcpy(flag_parse, ", armored (grade 1)", sizeof(flag_parse));

          if (IS_SET(GET_CYBERWARE_FLAGS(j), ARMS_MOD_STRENGTH_MOD1))
            strlcpy(flag_parse, ", strengthening (grade 1)", sizeof(flag_parse));
          else if (IS_SET(GET_CYBERWARE_FLAGS(j), ARMS_MOD_STRENGTH_MOD2))
            strlcpy(flag_parse, ", strengthening (grade 2)", sizeof(flag_parse));
          else if (IS_SET(GET_CYBERWARE_FLAGS(j), ARMS_MOD_STRENGTH_MOD3))
            strlcpy(flag_parse, ", strengthening (grade 3)", sizeof(flag_parse));

          if (IS_SET(GET_CYBERWARE_FLAGS(j), ARMS_MOD_QUICKNESS_MOD1))
            strlcpy(flag_parse, ", quickening (grade 1)", sizeof(flag_parse));
          else if (IS_SET(GET_CYBERWARE_FLAGS(j), ARMS_MOD_QUICKNESS_MOD2))
            strlcpy(flag_parse, ", quickening (grade 2)", sizeof(flag_parse));
          else if (IS_SET(GET_CYBERWARE_FLAGS(j), ARMS_MOD_QUICKNESS_MOD3))
            strlcpy(flag_parse, ", quickening (grade 3)", sizeof(flag_parse));
          */

          if (IS_SET(GET_CYBERWARE_FLAGS(j), ARMS_MOD_GYROMOUNT))
            strlcpy(flag_parse, ", gyromount-enabled", sizeof(flag_parse));
          break;
        case CYB_DERMALSHEATHING:
          if (GET_CYBERWARE_FLAGS(j))
            strlcpy(flag_parse, " ruthenium-coated", sizeof(flag_parse));
          break;
      }

      snprintf(ENDOF(buf), sizeof(buf) - strlen(buf), "%s-grade%s %s^n that uses ^c%.2f^n essence when installed.",
              decap_cyber_grades[GET_CYBERWARE_GRADE(j)],
              flag_parse,
              decap_cyber_types[GET_CYBERWARE_TYPE(j)],
              ((float) calculate_ware_essence_or_index_cost(ch, j) / 100));
      
      switch (GET_CYBERWARE_TYPE(j)) {
        case CYB_HANDRAZOR:
          snprintf(ENDOF(buf), sizeof(buf) - strlen(buf), "\r\nIt has a damage code of ^c(STR%s)L^n.", IS_SET(GET_CYBERWARE_FLAGS(j), 1 << CYBERWEAPON_IMPROVED) ? "+2" : "");
          break;
        case CYB_FIN:
        case CYB_CLIMBINGCLAWS:
        case CYB_FOOTANCHOR:
          snprintf(ENDOF(buf), sizeof(buf) - strlen(buf), "\r\nIt has a damage code of ^c(STR-1)L^n.");
          break;
        case CYB_FANGS:
          snprintf(ENDOF(buf), sizeof(buf) - strlen(buf), "\r\nIt has a damage code of ^c(STR+1)L^n and ^c-1 reach^n.");
          break;
        case CYB_HORNS:
          snprintf(ENDOF(buf), sizeof(buf) - strlen(buf), "\r\nIt has a damage code of ^c(STR+1)M^n and ^c-1 reach^n.");
          break;
        case CYB_HANDBLADE:
          snprintf(ENDOF(buf), sizeof(buf) - strlen(buf), "\r\nIt has a damage code of ^c(STR+3)L^n.");
          break;
        case CYB_HANDSPUR:
          snprintf(ENDOF(buf), sizeof(buf) - strlen(buf), "\r\nIt has a damage code of ^c(STR)M^n.");
          break;
        case CYB_BONELACING:
          snprintf(ENDOF(buf), sizeof(buf) - strlen(buf), "\r\nIt converts your unarmed attacks to deal ^c(STR+%d)M^n damage.", bone_lacing_power_lookup[GET_CYBERWARE_LACING_TYPE(j)]);
          break;
      }

      if (GET_CYBERWARE_TYPE(j) == CYB_EYES) {
        char eye_bits[1000];

        if (IS_SET(GET_CYBERWARE_FLAGS(j), EYE_CYBEREYES)) {
          strlcat(buf, "\r\n\r\nAs a set of replacement cybereyes, it has the following features: ", sizeof(buf));
          REMOVE_BIT(GET_CYBERWARE_FLAGS(j), EYE_CYBEREYES);
          sprintbit(GET_CYBERWARE_FLAGS(j), eyemods, eye_bits, sizeof(eye_bits));
          SET_BIT(GET_CYBERWARE_FLAGS(j), EYE_CYBEREYES);
        } else {
          strlcat(buf, "\r\n\r\nAs a standalone retinal modification, it has the following features: ", sizeof(buf));
          sprintbit(GET_CYBERWARE_FLAGS(j), eyemods, eye_bits, sizeof(eye_bits));
        }

        snprintf(ENDOF(buf), sizeof(buf) - strlen(buf), "\r\n  ^c%s^n.", eye_bits);
      }

      if (IS_OBJ_STAT(j, ITEM_EXTRA_MAGIC_INCOMPATIBLE)) {
        strlcat(buf, "\r\n^yIt is incompatible with magic.^n", sizeof(buf));
      }
      break;
    case ITEM_WORKSHOP:
      snprintf(ENDOF(buf), sizeof(buf) - strlen(buf), "It is a ^c%s^n designed for ^c%s^n.",
              kit_workshop_facility[GET_WORKSHOP_GRADE(j)],
              workshops[GET_WORKSHOP_TYPE(j)]);
      if (GET_WORKSHOP_TYPE(j) == TYPE_AMMO) {
        snprintf(ENDOF(buf), sizeof(buf) - strlen(buf), " It's specialized for ^c%s^n rounds.",
                 ammo_type[GET_WORKSHOP_AMMOKIT_TYPE(j)].name);
      }
      break;
    case ITEM_FOCUS:
      snprintf(ENDOF(buf), sizeof(buf) - strlen(buf), "It is a ^c%s^n focus of force ^c%d^n.",
               foci_type[GET_FOCUS_TYPE(j)],
               GET_FOCUS_FORCE(j));
      break;
    case ITEM_SPELL_FORMULA:
      if (SPELL_HAS_SUBTYPE(GET_SPELLFORMULA_SPELL(j)))
      {
        snprintf(ENDOF(buf), sizeof(buf) - strlen(buf), "It is a ^cforce-%d %s (%s)^n designed for ^c%s^n mages.",
                 GET_SPELLFORMULA_FORCE(j),
                 spells[GET_SPELLFORMULA_SPELL(j)].name,
                 attributes[GET_SPELLFORMULA_SUBTYPE(j)],
                 GET_SPELLFORMULA_TRADITION(j) ? "Shamanic" : "Hermetic");
      } else {
        snprintf(ENDOF(buf), sizeof(buf) - strlen(buf), "It is a ^cforce-%d %s^n designed for ^c%s^n mages.",
                 GET_SPELLFORMULA_FORCE(j),
                 spells[GET_SPELLFORMULA_SPELL(j)].name,
                 GET_SPELLFORMULA_TRADITION(j) ? "Shamanic" : "Hermetic");
      }
      break;
    case ITEM_PART:
      snprintf(ENDOF(buf), sizeof(buf) - strlen(buf), "It is %s rating-^C%d^n ^c%s^n designed for MPCP ^c%d^n decks. It will cost %d nuyen in parts and %d nuyen in chips to build.",
               !GET_PART_DESIGN_COMPLETION(j) ? "a not-yet-designed" : AN(parts[GET_PART_TYPE(j)].name),
               GET_PART_RATING(j),
               parts[GET_PART_TYPE(j)].name,
               GET_PART_TARGET_MPCP(j),
               GET_PART_PART_COST(j),
               GET_PART_CHIP_COST(j)
             );
      break;
    case ITEM_CUSTOM_DECK:
      snprintf(ENDOF(buf), sizeof(buf) - strlen(buf), "You should EXAMINE this deck, or jack in and view its SOFTWARE.");
      break;
    case ITEM_DRUG:
      snprintf(ENDOF(buf), sizeof(buf) - strlen(buf), "It contains ^c%d^n dose%s of ^c%s^n, which ",
               GET_OBJ_DRUG_DOSES(j),
               GET_OBJ_DRUG_DOSES(j) != 1 ? "s" : "",
               drug_types[GET_OBJ_DRUG_TYPE(j)].name);

      switch (GET_OBJ_DRUG_TYPE(j)) {
        case DRUG_ACTH:
          strlcat(buf, "activates adrenal pumps.", sizeof(buf));
          break;
        case DRUG_HYPER:
          strlcat(buf, "induces vertigo and sensory overload.", sizeof(buf));
          break;
        case DRUG_JAZZ:
          strlcat(buf, "increases your Quickness by 2 and initiative by +1d6.", sizeof(buf));
          break;
        case DRUG_KAMIKAZE:
          strlcat(buf, "is a battle stimulant, granting +1 to Body, Quickness, Willpower; +2 to Strength; and +1d6 initiative.", sizeof(buf));
          break;
        case DRUG_PSYCHE:
          strlcat(buf, "grants +1 Intelligence for a long time.", sizeof(buf));
          break;
        case DRUG_BLISS:
          strlcat(buf, "is a tranquilizing narcotic that reduces Reaction by 1, adds +1 to all roll difficulties (aka TN; lower is better), and grants pain resistance.", sizeof(buf));
          break;
        case DRUG_BURN:
          strlcat(buf, "is a synthahol intoxicant beverage.", sizeof(buf));
          break;
        case DRUG_CRAM:
          strlcat(buf, "is an amphetamine that adds 1 to Reaction and 1d6 to initiative.", sizeof(buf));
          break;
        case DRUG_NITRO:
          strlcat(buf, "is a powerful intoxicant that can easily kill its user. It grants +2 Strength and Willpower, as well as greater pain resistance.", sizeof(buf));
          break;
        case DRUG_NOVACOKE:
          strlcat(buf, "a highly-addictive coca-derived stimulant. It grants +1 Reaction and Charisma and minor pain resistance.", sizeof(buf));
          break;
        case DRUG_ZEN:
          strlcat(buf, "a psychedelic hallucinogen. It causes -2 Reaction and +1 physical roll difficulties (aka TN; lower is better), but also grants +1 Willpower.", sizeof(buf));
          break;
        default:
          snprintf(buf, sizeof(buf), "SYSERR: Unknown drug type %d in probe!", GET_OBJ_DRUG_TYPE(j));
          mudlog(buf, ch, LOG_SYSLOG, TRUE);
          break;
      }

      break;
    case ITEM_MAGIC_TOOL:
      snprintf(ENDOF(buf), sizeof(buf) - strlen(buf), "It is a ^crating-^c%d %s^n.", GET_OBJ_VAL(j, 1), magic_tool_types[GET_OBJ_VAL(j, 0)]);
      break;
    case ITEM_RADIO:
#ifdef ENABLE_RADIO_CRYPT
      snprintf(ENDOF(buf), sizeof(buf) - strlen(buf), "It has a ^c%d/5^n range and can encrypt and decrypt signals up to crypt level ^c%d^n.",
               GET_RADIO_FREQ_RANGE(j), GET_RADIO_MAX_CRYPT(j));
#else
      snprintf(ENDOF(buf), sizeof(buf) - strlen(buf), "It has a ^c%d/5^n range and transmits via secure Shadowlands encryption.",
               GET_RADIO_FREQ_RANGE(j));
#endif
      break;
    case ITEM_GUN_ACCESSORY:
      if (GET_ACCESSORY_TYPE(j) == ACCESS_SMARTGOGGLE) { /* pass */ }
      else if (GET_ACCESSORY_TYPE(j) == ACCESS_SMARTLINK || GET_ACCESSORY_TYPE(j) == ACCESS_GASVENT) {
        snprintf(ENDOF(buf), sizeof(buf) - strlen(buf), "It is a ^crating-%d %s^n that attaches to the ^c%s^n of a firearm.",
                 GET_ACCESSORY_TYPE(j) == ACCESS_GASVENT ? -GET_ACCESSORY_RATING(j) : GET_ACCESSORY_RATING(j), // gas vents have negative vals
                 gun_accessory_types[GET_ACCESSORY_TYPE(j)],
                 gun_accessory_locations[GET_ACCESSORY_ATTACH_LOCATION(j)]
                );
      }
      else {
        snprintf(ENDOF(buf), sizeof(buf) - strlen(buf), "It is %s ^c%s^n that attaches to the ^c%s^n of a firearm.",
                 AN(gun_accessory_types[GET_ACCESSORY_TYPE(j)]),
                 gun_accessory_types[GET_ACCESSORY_TYPE(j)],
                 gun_accessory_locations[GET_ACCESSORY_ATTACH_LOCATION(j)]
                );
      }
      switch (GET_ACCESSORY_TYPE(j)) {
        case ACCESS_BIPOD:
          snprintf(ENDOF(buf), sizeof(buf) - strlen(buf), "\r\nIt adds ^c%d^n points of recoil compensation when used while prone.", RECOIL_COMP_VALUE_BIPOD);
          break;
        case ACCESS_TRIPOD:
          snprintf(ENDOF(buf), sizeof(buf) - strlen(buf), "\r\nIt adds ^c%d^n points of recoil compensation when used while prone.", RECOIL_COMP_VALUE_TRIPOD);
          break;
        case ACCESS_SHOCKPAD:
          strlcat(buf, "\r\nIt adds ^c1^n point of recoil compensation.", sizeof(buf));
          break;
        case ACCESS_SMARTLINK:
          strlcat(buf, "\r\nIt makes it easier to hit opponents, provided you have the Smartlink cyberware or goggles.", sizeof(buf));
          break;
        case ACCESS_SCOPE:
          // These don't have built-in effects, check the affs.
          break;
        case ACCESS_GASVENT:
          snprintf(ENDOF(buf), sizeof(buf) - strlen(buf), "\r\nIt adds ^c%d^n points of recoil compensation, but once installed can only be removed with a gunsmithing workshop.", -GET_ACCESSORY_RATING(j));
          break;
        case ACCESS_SILENCER:
          strlcat(buf, "\r\nIt muffles the report of weapons that fire in Single-Shot or Semi-Automatic mode.", sizeof(buf));
          break;
        case ACCESS_SOUNDSUPP:
          strlcat(buf, "\r\nIt muffles the report of weapons that fire in Burst Fire or Full Auto mode.", sizeof(buf));
          break;
        case ACCESS_SMARTGOGGLE:
          strlcat(buf, "\r\nIt enables the use of Smartlink attachments, though at a lower efficiency than the cyberware would.", sizeof(buf));
          break;
        case ACCESS_BAYONET:
          strlcat(buf, "\r\nIt allows you to use the Pole Arms skill when defending against melee clashes. It also allows you to ^WEJECT^n your magazine to do a bayonet charge.", sizeof(buf));
          break;
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
      switch (GET_DECK_ACCESSORY_TYPE(j)) {
        case TYPE_FILE:
          snprintf(ENDOF(buf), sizeof(buf) - strlen(buf), "This file requires ^c%d^n units of space.", GET_DECK_ACCESSORY_FILE_SIZE(j));
          break;
        case TYPE_UPGRADE:
          if (GET_DECK_ACCESSORY_UPGRADE_COMPONENT(j) == DUPGRADE_HITCHER_JACK) {
            strlcat(buf, "This cyberdeck upgrade adds a ^chitcher jack^n.", sizeof(buf));
          } else {
            snprintf(ENDOF(buf), sizeof(buf) - strlen(buf), "This cyberdeck upgrade affects ^c%s^n with a rating of ^c%d^n.",
                     deck_accessory_upgrade_types[GET_DECK_ACCESSORY_UPGRADE_COMPONENT(j)],
                     GET_DECK_ACCESSORY_UPGRADE_RATING(j));
            if (GET_DECK_ACCESSORY_UPGRADE_COMPONENT(j) == DUPGRADE_REPLACEMENT_MPCP) {
              snprintf(ENDOF(buf), sizeof(buf) - strlen(buf), "\r\nIt is designed for MPCP rating ^c%d^n.", GET_DECK_ACCESSORY_UPGRADE_TARGET_MPCP(j));
            }
          }
          break;
        case TYPE_COMPUTER:
          snprintf(ENDOF(buf), sizeof(buf) - strlen(buf), "This computer has ^c%d^n units of active memory and ^c%d^n units of storage memory.",
                   GET_DECK_ACCESSORY_COMPUTER_ACTIVE_MEMORY(j),
                   get_device_total_memory(j));
          break;
        case TYPE_PARTS:
          snprintf(ENDOF(buf), sizeof(buf) - strlen(buf), "This pack of parts contains ^c%d^n units of ^c%s^n.",
                   GET_OBJ_COST(j),
                   GET_DECK_ACCESSORY_PARTS_IS_CHIPS(j) == 0 ? "deckbuilding parts" : "memory chips, which are use for both deckbuilding and cooking programs");
          break;
        case TYPE_COOKER:
          snprintf(ENDOF(buf), sizeof(buf) - strlen(buf), "It is a rating-^c%d^n chip cooker, used to burn completed programs to physical chips for use in cyberdecks.", GET_OBJ_VAL(j, 1));
          break;
        default:
          mudlog_vfprintf(ch, LOG_SYSLOG, "SYSERR: Unknown ITEM_DECK_ACCESSORY type %d passed to probe command.", GET_DECK_ACCESSORY_TYPE(j));
          break;
      }
      break;
    case ITEM_MOD:
      snprintf(ENDOF(buf), sizeof(buf) - strlen(buf), "It is %s ^c%s^n upgrade", AN(mod_types[GET_OBJ_VAL(j, 0)].name), mod_types[GET_OBJ_VAL(j, 0)].name);

      // Val 1
      if (GET_VEHICLE_MOD_TYPE(j) == TYPE_MOUNT) {
        snprintf(ENDOF(buf), sizeof(buf) - strlen(buf), " that adds %s ^c%s^n.", AN(mount_types[GET_OBJ_VAL(j, 1)]), mount_types[GET_OBJ_VAL(j, 1)]);
      } else {
        snprintf(ENDOF(buf), sizeof(buf) - strlen(buf), " that takes up ^c%d^n load space.", GET_OBJ_VAL(j, 1));
      }

      // Val 2
      if (GET_VEHICLE_MOD_TYPE(j) == MOD_ENGINE) {
        // engine type
        snprintf(ENDOF(buf), sizeof(buf) - strlen(buf), "\r\nIt is %s ^c%s^n engine.",
                 AN(engine_types[GET_VEHICLE_MOD_RATING(j)]),
                 engine_types[GET_VEHICLE_MOD_RATING(j)]);
      } else if (GET_VEHICLE_MOD_TYPE(j) == MOD_RADIO) {
        // radio range 0-5
#ifdef ENABLE_RADIO_CRYPT
        snprintf(ENDOF(buf), sizeof(buf) - strlen(buf), "\r\nIt has a ^c%d/5^n range and can encrypt and decrypt signals up to crypt level ^c%d^n.",
                 GET_VEHICLE_MOD_RATING(j),
                 GET_VEHICLE_MOD_RADIO_MAX_CRYPT(j));
#else
        snprintf(ENDOF(buf), sizeof(buf) - strlen(buf), "\r\nIt has a ^c%d/5^n range and transmits via secure Shadowlands encryption.",
                 GET_VEHICLE_MOD_RATING(j));
#endif
      } else if (GET_VEHICLE_MOD_LOCATION(j) == MOD_GRABBER) {
        // grabber arm
        snprintf(ENDOF(buf), sizeof(buf) - strlen(buf), "\r\nIt has a lifting capacity of ^c%d^n kilograms.", GET_VEHICLE_MOD_RATING(j));
      } else {
        snprintf(ENDOF(buf), sizeof(buf) - strlen(buf), "\r\nIt functions at rating ^c%d^n.", GET_VEHICLE_MOD_RATING(j));
      }

      // Val 5
      sprintbit(GET_VEHICLE_MOD_ENGINE_BITS(j), engine_types, buf2, sizeof(buf2));
      snprintf(ENDOF(buf), sizeof(buf) - strlen(buf), "\r\nIt is compatible with the following engine types:\r\n^c  %s^n", buf2);

      // Vals 4 and 6
      sprintbit(GET_VEHICLE_MOD_DESIGNED_FOR_FLAGS(j), veh_types, buf2, sizeof(buf2));
      if (GET_VEHICLE_MOD_TYPE(j) == TYPE_MOUNT) {
        strlcat(buf, "\r\nIt fits all vehicles and installs to the mount slot.\r\n", sizeof(buf));
      } else {
        snprintf(ENDOF(buf), sizeof(buf) - strlen(buf), "\r\nIt has been designed to fit vehicles of type: ^c%s^n, and installs to the ^c%s^n.",
                 buf2,
                 mod_name[GET_VEHICLE_MOD_LOCATION(j)]);
      }

      break;
    case ITEM_DESIGN:
      if (GET_OBJ_VAL(j, 0) == 5) {
        snprintf(ENDOF(buf), sizeof(buf) - strlen(buf), "This design is for a ^crating-%d %s (%s)^n program. It requires ^c%d^n units of storage.\r\n",
                GET_DESIGN_RATING(j),
                programs[GET_DESIGN_PROGRAM(j)].name,
                GET_WOUND_NAME(GET_DESIGN_PROGRAM_WOUND_LEVEL(j)),
                (int) ((GET_DESIGN_RATING(j) * GET_DESIGN_RATING(j)) * attack_multiplier[GET_DESIGN_PROGRAM_WOUND_LEVEL(j)] * 1.1));
      } else {
        snprintf(ENDOF(buf), sizeof(buf) - strlen(buf), "This design is for a ^crating-%d %s^n program. It requires ^c%d^n units of storage.\r\n",
                GET_DESIGN_RATING(j),
                programs[GET_DESIGN_PROGRAM(j)].name,
                (int) ((GET_DESIGN_RATING(j) * GET_DESIGN_RATING(j)) * programs[GET_DESIGN_PROGRAM(j)].multiplier * 1.1));
      }
      break;
    case ITEM_GUN_AMMO:
      if (GET_OBJ_VAL(j, 3))
        snprintf(ENDOF(buf), sizeof(buf) - strlen(buf), "It has %d/%d %s round%s of %s ammunition left.\r\n", GET_OBJ_VAL(j, 0), GET_OBJ_VAL(j, 0) +
                     GET_OBJ_VAL(j, 3), ammo_type[GET_OBJ_VAL(j, 2)].name,GET_OBJ_VAL(j, 0) != 1 ? "s" : "",
                     weapon_types[GET_OBJ_VAL(j, 1)]);
      else
        snprintf(ENDOF(buf), sizeof(buf) - strlen(buf), "It has %d %s round%s of %s ammunition left.\r\n", GET_OBJ_VAL(j, 0),
                     ammo_type[GET_OBJ_VAL(j, 2)].name,GET_OBJ_VAL(j, 0) != 1 ? "s" : "",
                     weapon_types[GET_OBJ_VAL(j, 1)]);
      break;
    case ITEM_CREATIVE_EFFORT:
      if (IS_SENATOR(ch)) {
        const char *author_name = get_player_name(GET_ART_AUTHOR_IDNUM(j));
        snprintf(ENDOF(buf), sizeof(buf) - strlen(buf), "It was created by ^c%s^n (%d).\r\n", author_name, GET_ART_AUTHOR_IDNUM(j));
        delete [] author_name;
      } else {
        strlcat(buf, "Nothing stands out about this item's OOC values. Try EXAMINE it instead.", sizeof(buf));
      }
      break;
    case ITEM_OTHER:
      if (GET_OBJ_VNUM(j) == OBJ_NEOPHYTE_SUBSIDY_CARD) {
        snprintf(ENDOF(buf), sizeof(buf) - strlen(buf), "It is bonded to %s and has %d nuyen remaining on it.\r\n",
                GET_IDNUM(ch) == GET_OBJ_VAL(j, 0) ? "you" : "someone else",
                GET_OBJ_VAL(j, 1));
        break;
      }
      if (GET_OBJ_VNUM(j) == OBJ_VEHCONTAINER) {
        strlcat(buf, "\r\nIt's a carried vehicle. You can ##^WDROP^n it on any road or garage.", sizeof(buf));
        break;
      }
      if (GET_OBJ_VNUM(j) == OBJ_SHOPCONTAINER) {
        strlcat(buf, "\r\nIt's a packaged-up bit of cyberware or bioware. See ##^WHELP CYBERDOC^n for what you can do with it.", sizeof(buf));
        break;
      }
      if (GET_OBJ_VNUM(j) == OBJ_ANTI_DRUG_CHEMS) {
        snprintf(ENDOF(buf), sizeof(buf) - strlen(buf), "\r\nIt's a bottle of anti-craving chemicals with ^c%d^n dose%s left. If you have it on you during ^Wguided^n withdrawal, you won't risk a fugue state. See ##^WHELP ADDICTION^n for more.",
                 GET_CHEMS_QTY(j),
                 GET_CHEMS_QTY(j) == 1 ? "" : "s");
        break;
      }
      if (j->obj_flags.bitvector.IsSet(AFF_WEARING_ACTIVE_DOCWAGON_RECEIVER)) {
        strlcat(buf, "\r\nIt allows you to tune in to the DocWagon emergency alert network and receive notifications of downed characters. See ##^WHELP PLAYERDOC^n for more.", sizeof(buf));
        break;
      }
      // fallthrough

    // All info about these is displayed when you examine them.
    case ITEM_GUN_MAGAZINE:
    case ITEM_CAMERA:
    case ITEM_PHONE:
    case ITEM_KEYRING:
      strlcat(buf, "Nothing stands out about this item's OOC values. Try EXAMINE it instead.", sizeof(buf));
      break;
    case ITEM_SHOPCONTAINER:
      send_to_char(ch, "%s is a shop container (see ^WHELP CYBERDOC^n for info). It contains:\r\n\r\n", capitalize(GET_OBJ_NAME(j)));
      do_probe_object(ch, j->contains, FALSE);
      return;
    default:
      strncpy(buf, "This item type has no probe string. Contact staff to request one.", sizeof(buf) - strlen(buf));
      break;
  }

  if (GET_OBJ_VNUM(j) == OBJ_MULTNOMAH_VISA || GET_OBJ_VNUM(j) == OBJ_CARIBBEAN_VISA) {
    idnum_t bound_to = get_soulbound_idnum(j);
    if (bound_to == GET_IDNUM(ch)) {
      strlcat(buf, " It has your picture on it.", sizeof(buf));
    } else if (bound_to) {
      strlcat(buf, " It has someone else's picture on it.", sizeof(buf));
    } else {
      strlcat(buf, " The picture has been covered with a peel-off film. Could be anyone's.", sizeof(buf));
    }
  }

  if (GET_OBJ_VNUM(j) == OBJ_STAFF_REBATE_FOR_CRAFTING) {
    if (GET_CRAFTING_TOKEN_IDNUM(j) <= 0) {
      strlcat(buf, " It can be used by anyone.", sizeof(buf));
    } else if (GET_CRAFTING_TOKEN_IDNUM(j) == GET_IDNUM(ch)) {
      strlcat(buf, " It can only be used by you.", sizeof(buf));
    } else {
      strlcat(buf, " You can't use it-- it belongs to someone else.", sizeof(buf));
    }
  }

  if (GET_OBJ_VNUM(j) == OBJ_HOLIDAY_GIFT) {
    if (GET_HOLIDAY_GIFT_ISSUED_TO(j) == GET_IDNUM(ch)) {
      strlcat(buf, " It is made out to you.", sizeof(buf));
    } else {
      strlcat(buf, " You can't open it-- it is made out to someone else.", sizeof(buf));
    }
  }

  if (GET_OBJ_VNUM(j) == OBJ_ONE_SHOT_HEALING_INJECTOR) {
    if (GET_HEALING_INJECTOR_ISSUED_TO(j) == GET_IDNUM(ch)) {
      strlcat(buf, " It can only be used by you.", sizeof(buf));
    } else {
      strlcat(buf, " You can't use it-- it belongs to someone else.", sizeof(buf));
    }
  }

  if (blocked_by_soulbinding(ch, j, FALSE)) {
    strlcat(buf, "\r\nIt is ^rsoulbound to someone else^n and cannot be used.", sizeof(buf));
  } else if (get_soulbound_idnum(j) == GET_IDNUM(ch)) {
    strlcat(buf, "\r\nIt is soulbound to you and cannot be used by anyone else.", sizeof(buf));
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
  bool is_dont_touch = IS_OBJ_STAT(j, ITEM_EXTRA_DONT_TOUCH);
  if (is_dont_touch)
    GET_OBJ_EXTRA(j).RemoveBit(ITEM_EXTRA_DONT_TOUCH);

  if (strcmp(GET_OBJ_EXTRA(j).ToString(), "0") != 0) {
    GET_OBJ_EXTRA(j).PrintBits(buf2, MAX_STRING_LENGTH, pc_readable_extra_bits, MAX_ITEM_EXTRA);
    snprintf(ENDOF(buf), sizeof(buf) - strlen(buf), "This object has the following extra features: ^c%s^n\r\n", buf2);
  }

  // Restore the dont_touch bit.
  if (is_dont_touch)
    GET_OBJ_EXTRA(j).SetBit(ITEM_EXTRA_DONT_TOUCH);

  if (GET_OBJ_TYPE(j) == ITEM_MOD) {
    strncpy(buf1, "This object modifies your vehicle in the following ways:\r\n  ^c", sizeof(buf1));
    for (i = 0; i < MAX_OBJ_AFFECT; i++) {
      if (j->affected[i].modifier) {
        sprinttype(j->affected[i].location, veh_aff, buf2, sizeof(buf2));
        snprintf(ENDOF(buf1), sizeof(buf1) - strlen(buf1), "%s %+d to %s", found++ ? "," : "",
                 j->affected[i].modifier, buf2);
      }
    }
  } else {
    strncpy(buf1, "This object modifies your character in the following ways when used:\r\n  ^c", sizeof(buf1));
    for (i = 0; i < MAX_OBJ_AFFECT; i++) {
      if (j->affected[i].modifier) {
        sprinttype(j->affected[i].location, apply_types, buf2, sizeof(buf2));
        snprintf(ENDOF(buf1), sizeof(buf1) - strlen(buf1), "%s %+d to %s", found++ ? "," : "",
                 j->affected[i].modifier, buf2);
      }
    }
  }

  if (found) {
    snprintf(ENDOF(buf), sizeof(buf) - strlen(buf), "%s^n\r\n", buf1);
  }

  strlcat(buf, get_command_hints_for_obj(j), sizeof(buf));

  if (IS_OBJ_STAT(j, ITEM_EXTRA_NERPS)) {
    strlcat(buf, "^YIt has been flagged NERPS, indicating it has no special coded effects.^n\r\n", sizeof(buf));
  }

  if (j->source_info) {
    snprintf(ENDOF(buf), sizeof(buf) - strlen(buf), "\r\nIt references data from the following source book(s), see HELP ^WSOURCEBOOK^n: %s^n\r\n", j->source_info);
  }

  send_to_char(buf, ch);
}

ACMD(do_examine)
{
  int i, skill = 0;
  struct char_data *tmp_char;
  struct obj_data *tmp_object;
  struct veh_data *found_veh = NULL;
  
  char *remainder = any_one_arg(argument, arg);

  if (!*arg) {
    send_to_char("Examine what?\r\n", ch);
    return;
  }
  if (subcmd == SCMD_EXAMINE) {
    skip_spaces(&remainder);
    look_at_target(ch, arg, remainder);
  }

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

      skill = get_br_skill_for_veh(found_veh);

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
      do_probe_object(ch, tmp_object, FALSE);
      return;
    }

    // Search cyber/bioware.
    for (struct obj_data *ware = ch->cyberware; ware; ware = ware->next_content) {
      if (keyword_appears_in_obj(arg, ware)) {
        send_to_char("^L(Probing your installed cyberware.)^n\r\n", ch);
        do_probe_object(ch, ware, FALSE);
        return;
      }
    }
    for (struct obj_data *ware = ch->bioware; ware; ware = ware->next_content) {
      if (keyword_appears_in_obj(arg, ware)) {
        send_to_char("^L(Probing your installed bioware.)^n\r\n", ch);
        do_probe_object(ch, ware, FALSE);
        return;
      }
    }

    // Found nothing.
    send_to_char("You're not wearing or carrying any such object, and there are no vehicles like that here.\r\n", ch);

    if (ch->in_room) {
      for (struct char_data *tmp_ch = ch->in_room->people; tmp_ch; tmp_ch = tmp_ch->next_in_room) {
        if (IS_NPC(tmp_ch) && MOB_HAS_SPEC(tmp_ch, shop_keeper)) {
          send_to_char("(To probe a shop's item, use the item's list index, like ^WPROBE #1^n.)\r\n", ch);
          break;
        }
      }
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
        GET_OBJ_TYPE(tmp_object) == ITEM_KEYRING ||
        GET_OBJ_TYPE(tmp_object) == ITEM_QUIVER ||
        (GET_OBJ_TYPE(tmp_object) == ITEM_WORN && tmp_object->contains)) {
      if (GET_OBJ_TYPE(tmp_object) == ITEM_CONTAINER && IS_SET(GET_CONTAINER_FLAGS(tmp_object), CONT_CLOSED)) {
        send_to_char("It is closed.\r\n", ch);
      } else if (!tmp_object->contains) {
        send_to_char("Looking inside reveals it to be empty.\r\n", ch);
      } else {
        if (GET_OBJ_TYPE(tmp_object) == ITEM_KEYRING) {
          send_to_char("Attached to the keyring is:\r\n", ch);
        } else {
          float weight_when_full =get_proto_weight(tmp_object) + GET_OBJ_VAL(tmp_object, 0);

          if (GET_OBJ_WEIGHT(tmp_object) >= weight_when_full * .9)
            send_to_char("It is bursting at the seams.\r\n", ch);
          else if (GET_OBJ_WEIGHT(tmp_object) >= weight_when_full / 2)
            send_to_char("It is more than half full.\r\n", ch);
          else
            send_to_char("It is less than half full.\r\n", ch);

          send_to_char("When you look inside, you see:\r\n", ch);
        }

        look_in_obj(ch, tmp_object, TRUE, 0);
        send_to_char("\r\n", ch);
      }
    } else if ((GET_OBJ_TYPE(tmp_object) == ITEM_WEAPON) &&
               (IS_GUN(GET_OBJ_VAL(tmp_object, 3)))) {
      for (i = ACCESS_LOCATION_TOP; i <= ACCESS_LOCATION_UNDER; ++i)
        if (GET_OBJ_VAL(tmp_object, i) >= 0) {
          send_to_char(ch, "There is %s attached to the %s of it.\r\n",
                       GET_OBJ_VAL(tmp_object, i) > 0 ? short_object(GET_OBJ_VAL(tmp_object, i), 2) : "nothing",
                       gun_accessory_locations[i - ACCESS_LOCATION_TOP]);
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
      if (!GET_ITEM_MONEY_IS_CREDSTICK(tmp_object))    // paper money
        send_to_char(ch, "There looks to be about %d nuyen.\r\n", GET_OBJ_VAL(tmp_object, 0));
      else {
        if (!IS_NPC(ch) && GET_ITEM_MONEY_CREDSTICK_OWNER_ID(tmp_object)) {
          if (belongs_to(ch, tmp_object)) {
            send_to_char("It has been activated by you and linked to one of your numbered bank accounts.\r\n", ch);
            send_to_char(ch, "The account display shows ^c%ld^n nuyen.\r\n", GET_BANK(ch));
          } else {
            send_to_char("It has been activated by someone else.\r\n", ch);
            send_to_char(ch, "The account display shows ^c%d^n nuyen.\r\n", GET_ITEM_MONEY_VALUE(tmp_object));
          }
        } else {
          send_to_char("It has not been activated.\r\n", ch);
          send_to_char(ch, "The account display shows ^c%d^n nuyen.\r\n", GET_ITEM_MONEY_VALUE(tmp_object));
        }
      }
    } else if (GET_OBJ_TYPE(tmp_object) == ITEM_GUN_MAGAZINE) {
      send_to_char(ch, "It has %d %s round%s left.\r\n", GET_OBJ_VAL(tmp_object, 9),
                   ammo_type[GET_OBJ_VAL(tmp_object, 2)].name,GET_OBJ_VAL(tmp_object, 9) != 1 ? "s" : "");
      send_to_char(ch, "It can hold a maximum of %d %s round%s.\r\n", GET_OBJ_VAL(tmp_object, 0), weapon_types[GET_OBJ_VAL(tmp_object, 1)],
                   GET_OBJ_VAL(tmp_object, 0) != 1 ? "s" : "");
    } else if (GET_OBJ_TYPE(tmp_object) == ITEM_DRUG) {
      send_to_char(ch, "It has %d dose%s of %s left.\r\n",
                   GET_OBJ_DRUG_DOSES(tmp_object),
                   GET_OBJ_DRUG_DOSES(tmp_object) == 1 ? "" : "s",
                   drug_types[GET_OBJ_DRUG_TYPE(tmp_object)].name);
    } else if (GET_OBJ_TYPE(tmp_object) == ITEM_GUN_AMMO) {
      if (GET_OBJ_VAL(tmp_object, 3))
        send_to_char(ch, "It has %d/%d %s round%s of %s ammunition left.\r\n", GET_OBJ_VAL(tmp_object, 0), GET_OBJ_VAL(tmp_object, 0) +
                     GET_OBJ_VAL(tmp_object, 3), ammo_type[GET_OBJ_VAL(tmp_object, 2)].name,GET_OBJ_VAL(tmp_object, 0) != 1 ? "s" : "",
                     weapon_types[GET_OBJ_VAL(tmp_object, 1)]);
      else
        send_to_char(ch, "It has %d %s round%s of %s ammunition left.\r\n", GET_OBJ_VAL(tmp_object, 0),
                     ammo_type[GET_OBJ_VAL(tmp_object, 2)].name,GET_OBJ_VAL(tmp_object, 0) != 1 ? "s" : "",
                     weapon_types[GET_OBJ_VAL(tmp_object, 1)]);
    } else if (GET_OBJ_VNUM(tmp_object) == 119 && GET_OBJ_VAL(tmp_object, 0) == GET_IDNUM(ch))
      snprintf(buf, sizeof(buf), "The card contains %d nuyen.\r\n", GET_OBJ_VAL(tmp_object, 1));
    else if (GET_OBJ_TYPE(tmp_object) == ITEM_DECK_ACCESSORY && GET_OBJ_VAL(tmp_object, 0) == TYPE_COOKER) {
      if (!tmp_object->contains)
        strncpy(buf, "The small LED is currently off.\r\n", sizeof(buf));
      else if (GET_DECK_ACCESSORY_COOKER_TIME_REMAINING(tmp_object))
        snprintf(buf, sizeof(buf), "The small LED is currently orange, indicating activity. The progress meter for cooking %s is at %d%%.\r\n",
                 GET_OBJ_NAME(tmp_object->contains),
                 (int)(((float)(GET_DECK_ACCESSORY_COOKER_ORIGINAL_TIME(tmp_object) - GET_DECK_ACCESSORY_COOKER_TIME_REMAINING(tmp_object)) / GET_DECK_ACCESSORY_COOKER_ORIGINAL_TIME(tmp_object)) * 100));
      else if (GET_OBJ_TIMER(tmp_object->contains) == -1)
        strncpy(buf, "The small LED is currently flashed ^rred^n, indicating a failed encode.\r\n", sizeof(buf));
      else
        strncpy(buf, "The small LED is currently ^ggreen^n, indicating a successful encode.\r\n", sizeof(buf));
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
              snprintf(ENDOF(buf), sizeof(buf) - strlen(buf), "%-40s  Type: ^c%-24s^n (rating ^c%d^n)\r\n",
                      GET_OBJ_NAME(soft),
                      parts[GET_OBJ_VAL(soft, 0)].name,
                      GET_PART_RATING(soft));
          send_to_char(buf, ch);
        }
      }
    } else if (GET_OBJ_TYPE(tmp_object) == ITEM_MAGIC_TOOL) {
      switch (GET_MAGIC_TOOL_TYPE(tmp_object)) {
        case TYPE_CIRCLE:
          {
            int initial_build_time = MAX(1, GET_MAGIC_TOOL_RATING(tmp_object) * 60);
            float completion_percentage = ((float)(initial_build_time - GET_MAGIC_TOOL_BUILD_TIME_LEFT(tmp_object)) / initial_build_time) * 100;

            send_to_char(ch, "It is a rating-%d circle built around the element of %s.\r\n",
                         GET_MAGIC_TOOL_RATING(tmp_object),
                         elements[GET_MAGIC_TOOL_TOTEM_OR_ELEMENT(tmp_object)].name);

            if (GET_MAGIC_TOOL_BUILD_TIME_LEFT(tmp_object) && GET_MAGIC_TOOL_OWNER(tmp_object) == GET_IDNUM(ch)) {
              send_to_char(ch, "It looks like you've completed around %.02f%% of it.\r\n", completion_percentage);
            }
          }
          break;
        case TYPE_LODGE:
          {
            int initial_build_time = MAX(1, GET_MAGIC_TOOL_RATING(tmp_object) * 300);
            float completion_percentage = ((float)(initial_build_time - GET_MAGIC_TOOL_BUILD_TIME_LEFT(tmp_object)) / initial_build_time) * 100;

            send_to_char(ch, "It is a rating-%d lodge dedicated to %s.\r\n",
                         GET_MAGIC_TOOL_RATING(tmp_object),
                         totem_types[GET_MAGIC_TOOL_TOTEM_OR_ELEMENT(tmp_object)]);

            if (GET_MAGIC_TOOL_BUILD_TIME_LEFT(tmp_object) && GET_MAGIC_TOOL_OWNER(tmp_object) == GET_IDNUM(ch)) {
              send_to_char(ch, "It looks like you've completed around %.02f%% of it.\r\n", completion_percentage);
            }
          }
          break;
        case TYPE_SUMMONING:
          send_to_char(ch, "There seems to be about %d nuyen worth.\r\n", GET_OBJ_COST(tmp_object));
          break;
        case TYPE_LIBRARY_CONJURE:
        case TYPE_LIBRARY_SPELL:
          send_to_char(ch, "It seems to have enough material to be classified as rating %d.\r\n", GET_MAGIC_TOOL_RATING(tmp_object));
          break;
      }
    } else if (GET_OBJ_TYPE(tmp_object) == ITEM_SPELL_FORMULA) {
      if (GET_OBJ_TIMER(tmp_object) < 0) {
        if (GET_OBJ_VAL(tmp_object, 8) == GET_IDNUM(ch)) {
          int timeleft = GET_SPELLFORMULA_TIME_LEFT(tmp_object);
          if (GET_OBJ_TIMER(tmp_object) == SPELL_DESIGN_FAILED_CODE)
            timeleft *= 2;
          if (GET_SPELLFORMULA_TIME_LEFT(tmp_object))
            send_to_char(ch, "You are about %d%% done.\r\n", (int)(((float)timeleft / (float)GET_SPELLFORMULA_INITIAL_TIME(tmp_object)) * -100 + 100));
          else
            send_to_char("You haven't started designing this spell yet.\r\n", ch);
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
      if (blocked_by_soulbinding(ch, tmp_object, FALSE)) {
        send_to_char("It is ^rsoulbound to someone else^n and cannot be used.\r\n", ch);
      } else if (get_soulbound_idnum(tmp_object) == GET_IDNUM(ch)) {
        send_to_char("It is ^csoulbound to you^n and cannot be used by anyone else.\r\n", ch);
      }
    }

    if (GET_OBJ_TYPE(tmp_object) == ITEM_CREATIVE_EFFORT) {
      send_to_char(ch, "^L(OOC: It's a custom object with no coded effect.)^n\r\n");
    } else if (GET_OBJ_TYPE(tmp_object) == ITEM_DESTROYABLE) {
      send_to_char(ch, "^L(OOC: You're able to ##^WDESTROY^L it.)^n\r\n");
    } else if (GET_OBJ_VNUM(tmp_object) > 1) {
      snprintf(buf, sizeof(buf), "You %s that %s ",
               GET_SKILL(ch, SKILL_POLICE_PROCEDURES) <= 0 ? "have no training in Police Procedures, but you guess" : "think",
               GET_OBJ_NAME(tmp_object));
      int targ = GET_LEGAL_NUM(tmp_object);
      if (targ <= 0)
        strlcat(buf, "is completely legal", sizeof(buf));
      else {
#ifdef ROLL_POLICE_PROCEDURES
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
#else // ROLL_POLICE_PROCEDURES is not defined
        if (GET_LEGAL_NUM(tmp_object) <= 3)
          strlcat(buf, "is highly illegal", sizeof(buf));
        else if (GET_LEGAL_NUM(tmp_object) <= 6)
          strlcat(buf, "is illegal", sizeof(buf));
        else strlcat(buf, "could be considered illegal", sizeof(buf));
#endif // ROLL_POLICE_PROCEDURES
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
    send_to_char("You are carrying one miserable nuyen.\r\n", ch);
  else {
    send_to_char(ch, "You are carrying %ld nuyen.\r\n", GET_NUYEN(ch));
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
    snprintf(pools, sizeof(pools), "  Ranged Dodge: %d%s\r\n  Damage Soak: %d\r\n  Offense: %d\r\n  Total Combat Dice: %d\r\n",
            GET_DODGE(ch), buf, GET_BODY_POOL(ch), GET_OFFENSE(ch), GET_COMBAT_POOL(ch));
  } else {
    snprintf(pools, sizeof(pools), "  Combat: %d     (Ranged Dodge: %d%s      Damage Soak: %d     Offense: %d)\r\n",
            GET_COMBAT_POOL(ch), GET_DODGE(ch), buf, GET_BODY_POOL(ch), GET_OFFENSE(ch));
  }
  if (GET_ASTRAL(ch) > 0)
    snprintf(ENDOF(pools), sizeof(pools) - strlen(pools), "  Astral: %d\r\n", GET_ASTRAL(ch));
  if (GET_HACKING(ch) > 0)
    snprintf(ENDOF(pools), sizeof(pools) - strlen(pools), "  Hacking: %d\r\n", GET_HACKING(ch));
  if (GET_MAGIC_POOL(ch) > 0) {
    bool over_spellpool_limit = GET_CASTING(ch) > GET_SKILL(ch, SKILL_SORCERY);

    if (PRF_FLAGGED(ch, PRF_SCREENREADER)) {
      snprintf(ENDOF(pools), sizeof(pools) - strlen(pools), "  Spell: %d\r\n  Casting: %d%s\r\n  Drain: %d\r\n  Defense: %d\r\n",
               GET_MAGIC_POOL(ch),
               GET_CASTING(ch),
               over_spellpool_limit ? " (warning: exceeds sorcery skill!)" : "",
               GET_DRAIN(ch),
               GET_SDEFENSE(ch));
      if (GET_METAMAGIC(ch, META_REFLECTING) == 2)
        snprintf(ENDOF(pools), sizeof(pools) - strlen(pools), "  Reflecting: %d\r\n", GET_REFLECT(ch));
    } else {
      snprintf(ENDOF(pools), sizeof(pools) - strlen(pools), "  Spell: %d      (Casting: %d %s   Drain: %d    Defense: %d",
               GET_MAGIC_POOL(ch),
               GET_CASTING(ch),
               over_spellpool_limit ? " (warning: exceeds sorcery skill!)" : "",
               GET_DRAIN(ch),
               GET_SDEFENSE(ch));
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
    strlcpy(position_string, "lying prone.", sizeof(position_string));
  else switch (GET_POS(ch)) {
    case POS_DEAD:
      strlcpy(position_string, "DEAD!", sizeof(position_string));
      break;
    case POS_MORTALLYW:
      snprintf(position_string, sizeof(position_string), "^rbleeding out^n! (^R%d^r ticks to death^n)^n ", GET_PC_SALVATION_TICKS(ch));
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
  if (SEES_ASTRAL(ch)) {
    if (ascii_friendly)
      return "You are astrally perceiving.";
    else
      return "You are astrally perceiving.\r\n";
  }

  if (ascii_friendly) {
    if (has_vision(ch, VISION_ULTRASONIC) && !affected_by_spell(ch, SPELL_STEALTH) && get_ch_in_room(ch)->silence[ROOM_NUM_SPELLS_OF_TYPE] <= 0)
      return "You have ultrasonic vision.";
  } else {
    if (has_vision(ch, VISION_ULTRASONIC)) {
      if (get_ch_in_room(ch)->silence[ROOM_NUM_SPELLS_OF_TYPE] > 0)
        return "Your ultrasonic vision is being suppressed by a field of silence here.\r\n";
      else if (affected_by_spell(ch, SPELL_STEALTH))
        return "Your ultrasonic vision is being suppressed by your stealth spell.\r\n";
      else
        return "You have ultrasonic vision.\r\n";
    }
  }

  if (has_vision(ch, VISION_THERMOGRAPHIC)) {
    if (ascii_friendly)
      return "You have thermographic vision.";
    else
      return "You have thermographic vision.\r\n";
  }

  if (has_vision(ch, VISION_LOWLIGHT)) {
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
    if (GET_BIOWARE_TYPE(bio) == BIO_PAINEDITOR && GET_BIOWARE_RATING(bio)) {
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
    if (GET_MAG(ch) != ch->real_abils.mag) {
      snprintf(ENDOF(buf2), sizeof(buf2) - strlen(buf2), "Magic: %d (base magic %d)\r\n", (int)(GET_MAG(ch) / 100), MAX(0, (int)(ch->real_abils.mag / 100)));
    } else {
      snprintf(ENDOF(buf2), sizeof(buf2) - strlen(buf2), "Magic: %d\r\n", (int)(GET_MAG(ch) / 100));
    }

    switch (GET_TRADITION(ch)) {
      case TRAD_SHAMANIC:
        snprintf(ENDOF(buf2), sizeof(buf2) - strlen(buf2), "You follow %s", totem_types[GET_TOTEM(ch)]);
        switch (GET_ASPECT(ch)) {
          case ASPECT_FULL:
            strlcat(buf2, ", and are a full shaman.\r\n", sizeof(buf2));
            break;
          case ASPECT_CONJURER:
            strlcat(buf2, ", and are an aspected conjurer.\r\n", sizeof(buf2));
            break;
          case ASPECT_SHAMANIST:
            strlcat(buf2, ", and are an aspected shamanist.\r\n", sizeof(buf2));
            break;
          case ASPECT_SORCERER:
            strlcat(buf2, ", and are an aspected sorcerer.\r\n", sizeof(buf2));
            break;
          default:
            strlcat(buf2, ".\r\n", sizeof(buf2));
            mudlog_vfprintf(ch, LOG_SYSLOG, "SYSERR: Unrecognized aspect %d in shamanic full score-- update the switch statement.", GET_ASPECT(ch));
            break;
        }
        break;
      case TRAD_ADEPT:
        strlcat(buf2, "You are a physical adept.\r\n", sizeof(buf2));
        break;
      case TRAD_HERMETIC:
        switch (GET_ASPECT(ch)) {
          case ASPECT_FULL:
            strlcat(buf2, "You are a full mage.\r\n", sizeof(buf2));
            break;
          case ASPECT_CONJURER:
            strlcat(buf2, "You are an aspected conjurer.\r\n", sizeof(buf2));
            break;
          case ASPECT_SHAMANIST:
            strlcat(buf2, "You are an aspected shamanist.\r\n", sizeof(buf2));
            break;
          case ASPECT_SORCERER:
            strlcat(buf2, "You are an aspected sorcerer.\r\n", sizeof(buf2));
            break;
          case ASPECT_ELEMEARTH:
            strlcat(buf2, "You are an aspected elementalist specializing in Earth magic.\r\n", sizeof(buf2));
            break;
          case ASPECT_ELEMAIR:
            strlcat(buf2, "You are an aspected elementalist specializing in Air magic.\r\n", sizeof(buf2));
            break;
          case ASPECT_ELEMFIRE:
            strlcat(buf2, "You are an aspected elementalist specializing in Fire magic.\r\n", sizeof(buf2));
            break;
          case ASPECT_ELEMWATER:
            strlcat(buf2, "You are an aspected elementalist specializing in Water magic.\r\n", sizeof(buf2));
            break;
          case ASPECT_EARTHMAGE:
            strlcat(buf2, "You are a full mage specializing in Earth magic.\r\n", sizeof(buf2));
            break;
          case ASPECT_AIRMAGE:
            strlcat(buf2, "You are a full mage specializing in Air magic.\r\n", sizeof(buf2));
            break;
          case ASPECT_FIREMAGE:
            strlcat(buf2, "You are a full mage specializing in Fire magic.\r\n", sizeof(buf2));
            break;
          case ASPECT_WATERMAGE:
            strlcat(buf2, "You are a full mage specializing in Water magic.\r\n", sizeof(buf2));
            break;
          default:
            mudlog_vfprintf(ch, LOG_SYSLOG, "SYSERR: Unrecognized aspect %d in hermetic full score-- update the switch statement.", GET_ASPECT(ch));
            break;
        }
        break;
      default:
        mudlog_vfprintf(ch, LOG_SYSLOG, "SYSERR: Received unknown tradition %d to score switch stanza 1!", GET_TRADITION(ch));
        break;
    }

    snprintf(ENDOF(buf2), sizeof(buf2) - strlen(buf2), "Initiation grade: %d\r\n", GET_GRADE(ch));
  }

  if (GET_REAL_REA(ch) != GET_REA(ch))
    snprintf(ENDOF(buf2), sizeof(buf2) - strlen(buf2), "Effective reaction: %d (base reaction %d)\r\n", GET_REA(ch), GET_REAL_REA(ch));
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

  if (IS_AFFECTED(ch, AFF_RUTHENIUM) || IS_AFFECTED(ch, AFF_IMP_INVIS) || IS_AFFECTED(ch, AFF_SPELLINVIS) || IS_AFFECTED(ch, AFF_SPELLIMPINVIS))
    strlcat(buf2, "You are invisible.\r\n", sizeof(buf2));

#ifdef ENABLE_HUNGER
  if (GET_COND(ch, COND_FULL) == 0)
    strlcat(buf2, "You are hungry.\r\n", sizeof(buf2));

  if (GET_COND(ch, COND_THIRST) == 0)
    strlcat(buf2, "You are thirsty.\r\n", sizeof(buf2));
#endif

  if (GET_COND(ch, COND_DRUNK) > 10) {
    if (affected_by_spell(ch, SPELL_DETOX)) {
      strlcat(buf2, "Your intoxication is suppressed by a detox spell.\r\n", sizeof(buf2));
    } else {
      strlcat(buf2, "You are intoxicated.\r\n", sizeof(buf2));
    }
  }

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

  if (IS_RIGGING(ch)) {
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
              "Speed: %s @ %3d km/h\r\n"
              "Accel:%3d\r\n"
              "Sig:%3d\r\n"
              "Sensors:%3d\r\n"
              "Load (used / total): %d / %d\r\n",
              GET_VEH_NAME(veh),
              veh->damage,
              (int)(GET_MENTAL(ch) / 100), (int)(GET_MAX_MENTAL(ch) / 100),
              GET_REA(ch),
              GET_INT(ch),
              GET_WIL(ch),
              veh->body,
              veh->armor,
              veh->autonav,
              veh->handling,
              veh_speeds[veh->cspeed], get_speed(veh),
              veh->accel,
              veh->sig,
              veh->sensor,
              (int) veh->usedload, (int) veh->load);
    } else {
      snprintf(buf, sizeof(buf), "You are rigging %s.\r\n", GET_VEH_NAME(veh));
      snprintf(ENDOF(buf), sizeof(buf) - strlen(buf), "Damage:^R%3d/10^n      Mental:^B%3d(%2d)^n\r\n"
              "  Reaction:%3d      Int:%3d\r\n"
              "       Wil:%3d      Bod:%3d\r\n"
              "     Armor:%3d  Autonav:%3d\r\n"
              "  Handling:%3d    Speed: %s @ %3d km/h\r\n"
              "     Accel:%3d      Sig:%3d\r\n"
              "   Sensors:%3d\r\n"
              "      Load:%d/%d\r\n",
              veh->damage, (int)(GET_MENTAL(ch) / 100),
              (int)(GET_MAX_MENTAL(ch) / 100), GET_REA(ch), GET_INT(ch),
              GET_WIL(ch), veh->body, veh->armor, veh->autonav,
              veh->handling, veh_speeds[veh->cspeed], get_speed(veh), veh->accel, veh->sig,
              veh->sensor,
              (int) veh->usedload, (int) veh->load);
    }
  } else {
    // Switches for the specific score types.
    skip_spaces(&argument);

    if (*argument) {
      int cmd_index;

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
      if (GET_BIOWARE_TYPE(bio) == BIO_PAINEDITOR && GET_BIOWARE_RATING(bio)) {
        pain_editor = TRUE;
        mental = 1000;
        physical = 1000;
        break;
      }
    if (pain_editor) {
      strlcat(buf, " ^YMasked by pain editor.         ^b/^L/\r\n", sizeof(buf));
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
      strlcat(buf, " ^YMasked by pain editor.         ^L/^b/\r\n", sizeof(buf));
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
    if (IS_AFFECTED(ch, AFF_RUTHENIUM) || IS_AFFECTED(ch, AFF_IMP_INVIS) || IS_AFFECTED(ch, AFF_SPELLINVIS) || IS_AFFECTED(ch, AFF_SPELLIMPINVIS))
      strlcpy(invisibility_string, "You are invisible.", sizeof(invisibility_string));
    else
      strlcpy(invisibility_string, "", sizeof(invisibility_string));


    char mage_string[50];
    strlcpy(mage_string, "", sizeof(mage_string));

    switch (GET_TRADITION(ch)) {
      case TRAD_HERMETIC:
      case TRAD_SHAMANIC:
        switch (GET_ASPECT(ch)) {
          case ASPECT_FULL:
            if (GET_TRADITION(ch) == TRAD_SHAMANIC)
              strlcat(mage_string, "Full Shaman", sizeof(mage_string));
            else
              strlcat(mage_string, "Full Mage", sizeof(mage_string));
            break;
          case ASPECT_CONJURER:
            strlcat(mage_string, "Conjurer", sizeof(mage_string));
            break;
          case ASPECT_SHAMANIST:
            strlcat(mage_string, "Shamanist", sizeof(mage_string));
            break;
          case ASPECT_SORCERER:
            strlcat(mage_string, "Sorcerer", sizeof(mage_string));
            break;
          case ASPECT_ELEMEARTH:
            strlcat(mage_string, "Earth-Aspected Mage", sizeof(mage_string));
            break;
          case ASPECT_ELEMAIR:
            strlcat(mage_string, "Air-Aspected Mage", sizeof(mage_string));
            break;
          case ASPECT_ELEMFIRE:
            strlcat(mage_string, "Fire-Aspected Mage", sizeof(mage_string));
            break;
          case ASPECT_ELEMWATER:
            strlcat(mage_string, "Water-Aspected Mage", sizeof(mage_string));
            break;
          case ASPECT_EARTHMAGE:
            strlcat(mage_string, "Earth-Specialized Full Mage", sizeof(mage_string));
            break;
          case ASPECT_AIRMAGE:
            strlcat(mage_string, "Air-Specialized Full Mage", sizeof(mage_string));
            break;
          case ASPECT_FIREMAGE:
            strlcat(mage_string, "Fire-Specialized Full Mage", sizeof(mage_string));
            break;
          case ASPECT_WATERMAGE:
            strlcat(mage_string, "Water-Specialized Full Mage", sizeof(mage_string));
            break;
          default:
            mudlog("SYSERR: Unrecognized aspect in score sheet-- update the switch statement.", ch, LOG_SYSLOG, TRUE);
            break;
        }

        if (GET_TRADITION(ch) == TRAD_SHAMANIC) {
          snprintf(ENDOF(mage_string), sizeof(mage_string) - strlen(mage_string), " following %s", totem_types[GET_TOTEM(ch)]);
        }
        break;
      case TRAD_ADEPT:
        strlcat(mage_string, "Physical Adept", sizeof(mage_string));
        break;
      default:
        if (IS_OTAKU(ch)) strlcat(mage_string, "Otaku", sizeof(mage_string));
        else strlcat(mage_string, "Mundane", sizeof(mage_string));
        break;
    }

    static char grade_string[50];
    if (GET_TRADITION(ch) != TRAD_MUNDANE || IS_OTAKU(ch))
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
                          MAX(0, ((int)ch->real_abils.mag / 100)), ((int)GET_MAG(ch) / 100), GET_COND(ch, COND_DRUNK) > 10 && !affected_by_spell(ch, SPELL_DETOX) ? "You are intoxicated." : "");
    snprintf(ENDOF(buf), sizeof(buf) - strlen(buf), "^L/^b/ ^nReaction      ^w%2d (^W%2d^w)    ^c%-41s^b/^L/\r\n",
                          GET_REAL_REA(ch), GET_REA(ch), out_of_body_string);
    snprintf(ENDOF(buf), sizeof(buf) - strlen(buf), "^b/^L/ ^nInitiative^w   [^W%2d^w+^W%d^rd6^n]    %-41s^L/^b/\r\n",
                          GET_REA(ch), 1 + GET_INIT_DICE(ch), mage_string);
    snprintf(ENDOF(buf), sizeof(buf) - strlen(buf), "^L/^b/ ^nArmor     ^w[ ^W%2d^rB^w/ ^W%2d^rI^w]    ^L%-17s^n                        ^b/^L/\r\n",
                          GET_BALLISTIC(ch), GET_IMPACT(ch), AFF_FLAGGED(ch, AFF_SNEAK) ? "You are sneaking." : "");
    snprintf(ENDOF(buf), sizeof(buf) - strlen(buf), "^b/^L/ ^nNuyen     ^w[^W%'9ld^w]    %11s   Social Bonus: [^W%3d / %3d^n]  ^L/^b/\r\n",
                          GET_NUYEN(ch), grade_string, GET_CONGREGATION_BONUS(ch), MAX_CONGREGATION_BONUS);
    strlcat(buf, "^L/^b/                                                                   ^b/^L/\r\n"
            "^L//^b//^L//^b//^L//^b//^L//^b//^L//^b//^L//^b//^L//^b//"
            "^L//^b//^L//^b//^L//^b//^L//^b//^L//^b//^L//^b//^L//"
            "^b//^L^L//^b//^L//^b//^L//^b//^L//^b/\r\n", sizeof(buf));
    strlcat(buf, "(See HELP SCORE for switches)\r\n", sizeof(buf));

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
  if (IS_RIGGING(ch)) {
    vehicle_inventory(ch);
    return;
  }

  send_to_char("You are carrying:\r\n", ch);
  list_obj_to_char(ch->carrying, ch, SHOW_MODE_IN_INVENTORY, TRUE, FALSE);

  int worn_total = 0;
  for (int wear_idx = 0; wear_idx < NUM_WEARS; wear_idx++) {
    if (GET_EQ(ch, wear_idx))
      worn_total++;
  }
  if (worn_total > 0)
    send_to_char(ch, "\r\nYou're using %d piece%s of ##^Wequipment^n.\r\n", worn_total, worn_total == 1 ? "" : "s");
  else
    send_to_char("\r\nYou're not using any ##^Wequipment^n.\r\n", ch);

  float ammo_weight = get_bulletpants_weight(ch);
  if (ammo_weight > 0)
    send_to_char(ch, "\r\nAdditionally, you have %.2f kilos of ammo in your ##^Wpockets^n.\r\n", ammo_weight);
  else
    send_to_char("\r\nYou have no ammo in your ##^Wpockets^n.\r\n", ch);
}

ACMD(do_cyberware)
{
  struct obj_data *obj;

  if (!ch->cyberware) {
    send_to_char("You have no cyberware.\r\n", ch);
    return;
  }

  send_to_char("^cYou have the following cyberware:^n\r\n", ch);
  for (obj = ch->cyberware; obj != NULL; obj = obj->next_content) {
    if (GET_CYBERWARE_TYPE(obj) == CYB_FINGERTIP) {
      if (obj->contains && GET_OBJ_TYPE(obj->contains) == ITEM_WEAPON && GET_HOLSTER_READY_STATUS(obj) ) {
        snprintf(buf, sizeof(buf), "%-32s ^Y(W) (R)^n Essence: ^c%0.2f^n\r\n", GET_OBJ_NAME(obj), ((float)calculate_ware_essence_or_index_cost(ch, obj) / 100));
        send_to_char(buf, ch);
        continue;
      }
      else if (obj->contains && GET_OBJ_TYPE(obj->contains) == ITEM_WEAPON && !GET_HOLSTER_READY_STATUS(obj)) {
        snprintf(buf, sizeof(buf), "%-36s ^Y(W)^n Essence: ^c%0.2f^n\r\n", GET_OBJ_NAME(obj), ((float)calculate_ware_essence_or_index_cost(ch, obj) / 100));
        send_to_char(buf, ch);
        continue;
      }
      else if (obj->contains) {
        snprintf(buf, sizeof(buf), "%-36s ^Y(F)^n Essence: ^c%0.2f^n\r\n", GET_OBJ_NAME(obj), ((float)calculate_ware_essence_or_index_cost(ch, obj) / 100));
        send_to_char(buf, ch);
        continue;
      }
    }

    char retraction_string[100];
    if (cyber_is_retractable(obj)) {
      if (GET_CYBERWARE_IS_DISABLED(obj)) {
        strlcpy(retraction_string, "  (retracted)", sizeof(retraction_string));
      } else {
        strlcpy(retraction_string, "  (extended)", sizeof(retraction_string));
      }
    } else if (GET_CYBERWARE_TYPE(obj) == CYB_ARMS && IS_SET(GET_CYBERWARE_FLAGS(obj), ARMS_MOD_GYROMOUNT)) {
      if (GET_CYBERWARE_IS_DISABLED(obj)) {
        strlcpy(retraction_string, "  (gyro deactivated)", sizeof(retraction_string));
      } else {
        strlcpy(retraction_string, "  (gyro activated)", sizeof(retraction_string));
      }
    } else {
      strlcpy(retraction_string, "", sizeof(retraction_string));
    }

    snprintf(buf, sizeof(buf), "%-40s Essence: ^c%0.2f^n%s\r\n",
             GET_OBJ_NAME(obj),
             (float)calculate_ware_essence_or_index_cost(ch, obj) / 100,
             retraction_string
           );
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
    if (GET_BIOWARE_TYPE(obj) == BIO_ADRENALPUMP && GET_BIOWARE_PUMP_ADRENALINE(obj) != 0) {
      send_to_char(ch, "%-33s (%4d) Rating: %-2d     Bioware Index: %0.2f\r\n",
                   GET_OBJ_NAME(obj),
                   GET_BIOWARE_PUMP_ADRENALINE(obj) *2,
                   GET_BIOWARE_RATING(obj),
                   ((float)calculate_ware_essence_or_index_cost(ch, obj) / 100));
    } else {
      send_to_char(ch, "%-40s Rating: %-2d     Bioware Index: %0.2f\r\n",
                   GET_OBJ_NAME(obj),
                   GET_BIOWARE_RATING(obj),
                   ((float)calculate_ware_essence_or_index_cost(ch, obj) / 100));
    }
  }
}

ACMD(do_equipment)
{
  int i, found = 0;

  bool filter_broken = FALSE;
  bool filter_keyword = FALSE;

  char message[1000];
  strlcpy(message, "You are using:\r\n", sizeof(message));

  skip_spaces(&argument);
  if (*argument) {
    if (is_abbrev(argument, "damaged") || is_abbrev(argument, "broken") || is_abbrev(argument, "destroyed")) {
      filter_broken = TRUE;
      strlcpy(message, "You are using the following broken items:\r\n", sizeof(message));
    } else {
      filter_keyword = TRUE;
      snprintf(message, sizeof(message), "You are using the following items matching '%s':\r\n", argument);
    }
  }
  
  send_to_char(message, ch);

  for (i = 0; i < NUM_WEARS; i++) {
    struct obj_data *obj = GET_EQ(ch, i);

    if (obj) {
      // Apply filters.
      if (filter_broken) {
        // Skip anything that's not damaged.
        if (GET_OBJ_CONDITION(obj) >= GET_OBJ_BARRIER(obj))
          continue;
      }

      if (filter_keyword) {
        // Skip anything that doesn't match the keyword.
        if (!keyword_appears_in_obj(argument, obj))
          continue;
      }

      // Send wearslot info.
      if (i == WEAR_WIELD || i == WEAR_HOLD) {
        if (IS_OBJ_STAT(obj, ITEM_EXTRA_TWOHANDS))
          send_to_char(hands[2], ch);
        else if (GET_OBJ_TYPE(obj) == ITEM_WEAPON) { // wielding something?
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
      } else {
        send_to_char(where[i], ch);
      }
      
      // Send object description.
      if (CAN_SEE_OBJ(ch, obj))
        show_obj_to_char(obj, ch, SHOW_MODE_OWN_EQUIPMENT);
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
  sh_int /* year, month, day,*/ hour, minute, pm;
  extern struct time_info_data time_info;
  extern const char *month_name[];
  struct obj_data *check;

  if (subcmd == SCMD_NORMAL) {
    if (GET_REAL_LEVEL(ch) >= LVL_BUILDER) {
      subcmd = SCMD_PRECISE;
      send_to_char("You use your staff powers to get an accurate time.\r\n", ch);
    } else {
      for (check = ch->cyberware; check; check = check->next_content) {
        if (GET_OBJ_VAL(check, 0) == CYB_MATHSSPU || (GET_OBJ_VAL(check, 0) == CYB_EYES && IS_SET(GET_OBJ_VAL(check, 3), EYE_CLOCK))) {
          subcmd = SCMD_PRECISE;
          send_to_char(ch, "You use %s for the most up-to-date time.\r\n", decapitalize_a_an(GET_OBJ_NAME(check)));
        }
      }
    }
  }

  // You can look at the dash clock in a vehicle.
  if (subcmd == SCMD_NORMAL && ch->in_veh) {
    subcmd = SCMD_PRECISE;
    send_to_char("You glance at the dash.\r\n", ch);
  }

  // If you still get scrub-grade time after the above checks...
  if (subcmd == SCMD_NORMAL) {
    struct room_data *room = get_ch_in_room(ch);
    if (room && ROOM_FLAGGED(room, ROOM_INDOORS))
      send_to_char("You guess at the time.\r\n", ch);
    else
      send_to_char("You glance up at the sky to help you guess the time.\r\n", ch);
  }

  // year = time_info.year % 100;
  // month = time_info.month + 1;
  // day = time_info.day + 1;
  hour = (time_info.hours % 12 == 0 ? 12 : time_info.hours % 12);
  minute = time_info.minute;
  pm = (time_info.hours >= 12);

  if (subcmd == SCMD_NORMAL)
    snprintf(buf, sizeof(buf), "Gameplay Time: %2d o'clock %s.\r\n", hour, pm ? "PM" : "AM");
  else
    snprintf(buf, sizeof(buf), "Gameplay Time: %2d:%s%d %s.\r\n", hour,
            minute < 10 ? "0" : "", minute, pm ? "PM" : "AM");

  send_to_char(buf, ch);

  // Add RP time.
  {
    time_t mytime = time(0);
    struct tm *modifiable_time = localtime(&mytime);

    bool pm = modifiable_time->tm_hour >= 12;
    hour = modifiable_time->tm_hour % 12;
    if (hour == 0)
      hour = 12;

    minute = modifiable_time->tm_min;

    mktime(modifiable_time);

    send_to_char(ch, "Roleplay Time: %2d:%.2d %s, %s, %s %d, 2064-ish.\r\n",
                 hour,
                 minute,
                 pm ? "PM" : "AM",
                 weekdays_tm_aligned[(int) modifiable_time->tm_wday],
                 month_name[modifiable_time->tm_mon],
                 modifiable_time->tm_mday
               );
  }
}

ACMD(do_weather)
{
  struct veh_data *veh;
  RIG_VEH(ch, veh);

  struct room_data *in_room = ch->in_room;
  struct veh_data *in_veh = ch->in_veh;
  if (veh) {
    ch->in_room = veh->in_room;
    ch->in_veh = veh->in_veh;
  }

  // DO NOT use FAILURE_CASE() here, since you MUST restore in_room and in_veh before returning control.

  if (!OUTSIDE(ch)) {
    send_to_char("You're pretty sure the ceiling won't start raining anytime soon.\r\n", ch);
    ch->in_room = in_room;
    ch->in_veh = in_veh;
    return;
  }

  int current_sky = GET_JURISDICTION(ch->in_room) == JURISDICTION_SECRET ? MAX(weather_info.sky, SKY_CLOUDY) : weather_info.sky;

  if (precipitation_is_snow(GET_JURISDICTION(ch->in_room))) {
    static const char *sky_look[] =
    {
      "cloudless",
      "cloudy",
      "snowy",
      "obscured by heavy snow"
    };

    send_to_char(ch, "The sky is %s and %s.\r\n",
                 sky_look[current_sky],
                 (weather_info.change >= 0 ? "you feel a cool breeze from north" : "the cold seeps into your bones"));
  }
  else {
    static const char *sky_look[] =
    {
      "cloudless",
      "cloudy",
      "rainy",
      "lit by flashes of lightning"
    };

    send_to_char(ch, "The sky is %s and %s.\r\n",
                 sky_look[current_sky],
                 (weather_info.change >= 0 ? "you feel a warm wind from south" : "your foot tells you bad weather is due"));
  }

  ch->in_room = in_room;
  ch->in_veh = in_veh;
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
  char query[MAX_STRING_LENGTH] = { 0 };
  char prepared_standard[MAX_STRING_LENGTH] = { 0 };
  char prepared_for_like[MAX_STRING_LENGTH] = { 0 };

  MYSQL_RES *res;
  MYSQL_ROW row;
  *help = '\0';

  if (!str_cmp(arg, "random")) {
    send_to_char(ch, "Looking up a random help topic...\r\n");
    strlcpy(query, "SELECT * FROM help_topic ORDER BY RAND() LIMIT 1;", sizeof(query));
  } else {
    // Pre-process our prepare_quotes.
    prepare_quotes(prepared_standard, arg, sizeof(prepared_standard) / sizeof(prepared_standard[0]));
    prepare_quotes(prepared_for_like, arg, sizeof(prepared_for_like) / sizeof(prepared_for_like[0]), FALSE, TRUE);

    // First strategy: Look for an exact match.
    snprintf(query, sizeof(query), "SELECT * FROM help_topic WHERE name='%s'", prepared_standard);
  }

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

  // Second strategy: Search for possible like-matches where words start with the entered text.
  #ifdef USE_MYSQL_8
  snprintf(query, sizeof(query), "SELECT * FROM help_topic WHERE name REGEXP '.*\\\\b%s.*' ORDER BY name ASC", prepared_for_like);
  #else
  snprintf(query, sizeof(query), "SELECT * FROM help_topic WHERE name REGEXP '.*[[:<:]]%s.*' ORDER BY name ASC", prepared_for_like);
  #endif

  if (mysql_wrapper(mysql, query)) {
    // If we don't find it here either, we know the file doesn't exist-- failure condition.
    snprintf(help, help_len, "No such help file exists (error case).\r\n");
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
  else if (x > 2) {
    // Prepare a just-in-case response with the topics that were found in the overbroad search.
    snprintf(help, help_len, "%d articles returned, please narrow your search. The topics found were:\r\n", x);
    while ((row = mysql_fetch_row(res))) {
      snprintf(ENDOF(help), help_len - strlen(help), "^W%s^n\r\n", row[0]);
    }
    mysql_free_result(res);

    // Try a lookup with just files that have the search string at the start of their title.
    snprintf(query, sizeof(query), "SELECT * FROM help_topic WHERE name LIKE '%s%%' ORDER BY name ASC", prepared_for_like);
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

bool characterNameCompareFunction(struct char_data *a, struct char_data *b) {return *(GET_CHAR_NAME(a)) < *(GET_CHAR_NAME(b));}

ACMD(do_quickwho)
{
  struct descriptor_data *d;
  struct char_data *tch;
  bool printed = FALSE;

  skip_spaces(&argument);
  strlcpy(buf, "Currently in-game: ", sizeof(buf));

  for (int candidate_level = LVL_MAX; candidate_level >= LVL_MORTAL; candidate_level--) {
    std::vector<struct char_data *> found_list = {};
    for (d = descriptor_list; d; d = d->next) {
      if (DESCRIPTOR_CONN_STATE_NOT_PLAYING(d))
        continue;

      // Assign tch to their most-relevant character.
      if (!(tch = d->original) && !(tch = d->character))
        continue;

      if (GET_LEVEL(tch) != candidate_level)
        continue;

      if (GET_INCOG_LEV(tch) > GET_LEVEL(ch))
        continue;

      // Add the target to the candidate list.
      found_list.push_back(tch);
    }

    // Now print the candidates if any.
    if (!found_list.empty()) {
      // Sort the list by character name, alphabetically.
      std::sort(found_list.begin(), found_list.end(), characterNameCompareFunction);

      // Print its contents.
      for (auto iter: found_list) {
        if (PRF_FLAGGED(ch, PRF_NOCOLOR)) {
          snprintf(ENDOF(buf), sizeof(buf) - strlen(buf), "%s%s%s",
                    printed ? ", " : "",
                    GET_CHAR_NAME((iter)),
                    GET_LEVEL((iter)) > LVL_MORTAL ? " (staff)" : ""
                  );
        } else {
          snprintf(ENDOF(buf), sizeof(buf) - strlen(buf), "%s%s%s^n",
                    printed ? ", " : "",
                    get_level_wholist_color(GET_LEVEL((iter))),
                    GET_CHAR_NAME((iter))
                  );
        }
        printed = TRUE;
      }
    }
  }
  send_to_char(ch, "%s\r\n", buf);
}

ACMD(do_who)
{
  struct descriptor_data *d;
  struct char_data *tch;
  int sort = LVL_MAX, num_can_see = 0, level = GET_LEVEL(ch);
  bool mortal = FALSE, hardcore = FALSE, quest = FALSE, pker = FALSE, immort = FALSE, ooc = FALSE, newbie = FALSE, drugs = FALSE, br = FALSE, rpe = FALSE;
  int output_header;
  int num_in_socialization_rooms = 0;

  skip_spaces(&argument);
  strlcpy(buf, argument, sizeof(buf));
  if (subcmd)
    level = 1;
  while (*buf) {
    half_chop(buf, arg, buf1, sizeof(buf1));

    if (is_abbrev(arg, "sort"))
      sort = LVL_MAX;
    else if (is_abbrev(arg, "questor"))
      quest = 1;
    else if (is_abbrev(arg, "pker")) {
#ifdef ENABLE_PK
      pker = 1;
#else
      send_to_char("PK is not enabled for this port.\r\n", ch);
      return;
#endif
    }
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
    else if (access_level(ch, LVL_BUILDER) && is_abbrev(arg, "rpe"))
      rpe = 1;
    else if (access_level(ch, LVL_BUILDER) && (is_abbrev(arg, "b/r") || is_abbrev(arg, "br")))
      br = 1;
    else if (access_level(ch, LVL_BUILDER) && is_abbrev(arg, "drugs"))
      drugs = 1;
    else {
      send_to_char(WHO_FORMAT, ch);
      return;
    }
    strlcpy(buf, buf1, sizeof(buf));
  }                             /* end while (parser) */

  *buf = '\0';
  output_header = 1;

  std::unordered_map<std::string, bool> unique_connections = {};

  for (; sort != 0; sort > 0 ? sort-- : sort++) {
    if ( sort == 1 )
      output_header = 1;
    for (d = descriptor_list; d; d = d->next) {
      unique_connections[std::string(d->host)] = TRUE;

      if (DESCRIPTOR_CONN_STATE_NOT_PLAYING(d))
        continue;

      if (d->original)
        tch = d->original;
      else if (!(tch = d->character))
        continue;

      if ((mortal && IS_SENATOR(tch)) ||
          (immort && !IS_SENATOR(tch)))
        continue;
      if (quest && !PRF_FLAGGED(tch, PRF_HIRED))
        continue;
      if (pker && !PRF_FLAGGED(tch, PRF_PKER))
        continue;
      if (hardcore && !PRF_FLAGGED(tch, PRF_HARDCORE))
        continue;
      if (sort > 0 && GET_LEVEL(tch) != sort)
        continue;
      if (ooc && (PRF_FLAGGED(tch, PRF_NOOOC) || PLR_FLAGGED(tch, PLR_NOT_YET_AUTHED)))
        continue;
      if (newbie && (!PLR_FLAGGED(tch, PLR_NEWBIE) || IS_PRESTIGE_CH(tch)))
        continue;
      if (drugs && !PLR_FLAGGED(tch, PLR_ENABLED_DRUGS))
        continue;
      if (br && !AFF_FLAGS(tch).AreAnySet(BR_TASK_AFF_FLAGS, ENDBIT))
        continue;
      if (rpe && !PLR_FLAGGED(tch, PLR_RPE))
        continue;
      if (GET_INCOG_LEV(tch) > GET_LEVEL(ch))
        continue;
      num_can_see++;

      if (ch_can_see_vict_in_where(ch, tch)) {
        num_in_socialization_rooms++;
      }

      if ( output_header ) {
        output_header = 0;
        if ( sort >= LVL_BUILDER )
          strlcat(buf, "\r\n^W     Staff ^L: ^wReach us on Discord!\r\n ^w---------------------------\r\n", sizeof(buf));
        else
          strlcat(buf, "\r\n^W   Players ^L: ^w(OOC Info)\r\n ^W---------------------------\r\n", sizeof(buf));
      }

      strlcpy(buf1, get_level_wholist_color(GET_LEVEL(tch)), sizeof(buf1));

      if (PRF_FLAGGED(tch, PRF_SHOWGROUPTAG) && GET_PGROUP_MEMBER_DATA(tch) && GET_PGROUP(tch)) {
        snprintf(buf2, sizeof(buf2), "%10s ^n:^N %s%s^N%s%s%s %s^n",
                (GET_WHOTITLE(tch) ? GET_WHOTITLE(tch) : ""),
                (GET_PGROUP(tch)->get_tag()),
                (strlen(GET_PGROUP(tch)->get_tag()) ? " " : ""),
                (GET_PRETITLE(tch) ? GET_PRETITLE(tch) : ""),
                (GET_PRETITLE(tch) && strlen(GET_PRETITLE(tch)) ? " " : ""),
                GET_CHAR_NAME(tch),
                GET_TITLE(tch));
      } else {
        snprintf(buf2, sizeof(buf2), "%10s ^n:^N %s%s%s %s^n",
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

      if (GET_TKE(tch) <= NEWBIE_KARMA_THRESHOLD && !IS_SENATOR(tch) && !IS_PRESTIGE_CH(tch)) {
        strlcat(buf1, GET_TITLE(tch) ? " ^y(Newbie)^n" : "^y(Newbie)^n", sizeof(buf1));
      }

      if (AFF_FLAGS(tch).AreAnySet(BR_TASK_AFF_FLAGS, ENDBIT))
        strlcat(buf1, GET_TITLE(tch) ? " (B/R)" : "(B/R)", sizeof(buf1));
      if (PRF_FLAGGED(tch, PRF_AFK))
        strlcat(buf1, " (AFK)", sizeof(buf1));
      if (PLR_FLAGGED(tch, PLR_RPE) && (level > LVL_MORTAL || PLR_FLAGGED(ch, PLR_RPE)))
        strlcat(buf1, " ^R(RPE)^n", sizeof(buf1));
      if (PLR_FLAGGED(tch, PLR_CYBERDOC) && GET_LEVEL(tch) <= LVL_MORTAL)
        strlcat(buf1, " ^c(Cyberdoc)^n", sizeof(buf1));

      /*
      if (PRF_FLAGGED(tch, PRF_NEWBIEHELPER) && (level > LVL_MORTAL || PRF_FLAGGED(ch, PRF_NEWBIEHELPER)))
        strlcat(buf1, " ^G(Newbie Helper)^n", sizeof(buf1));
      */

      if (level > LVL_MORTAL) {
        if (GET_INVIS_LEV(tch) && level >= GET_INVIS_LEV(tch))
          snprintf(ENDOF(buf1), sizeof(buf1) - strlen(buf1), " (i%d)", GET_INVIS_LEV(tch));
        if (GET_INCOG_LEV(tch))
          snprintf(ENDOF(buf1), sizeof(buf1) - strlen(buf1), " (c%d)", GET_INCOG_LEV(tch));
        if (PLR_FLAGGED(tch, PLR_MAILING))
          strlcat(buf1, " (mailing)", sizeof(buf1));
        else if (PLR_FLAGGED(tch, PLR_WRITING))
          strlcat(buf1, " (writing)", sizeof(buf1));
        if (PRF_FLAGGED(tch, PRF_QUESTOR))
          strlcat(buf1, " ^Y(questor)^n", sizeof(buf1));
        if (PLR_FLAGGED(tch, PLR_EDITING) || d->connected)
          strlcat(buf1, " (editing)", sizeof(buf1));
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

      if (!quest && PRF_FLAGGED(tch, PRF_HIRED))
        strlcat(buf1, " ^Y(hired)^n", sizeof(buf1));
      if (PRF_FLAGGED(tch, PRF_NOTELL))
        strlcat(buf1, " (!tell)", sizeof(buf1));
      if (PLR_FLAGGED(tch, PLR_KILLER))
        strlcat(buf1, " ^R(KILLER)^N", sizeof(buf1));
      if (PLR_FLAGGED(tch, PLR_BLACKLIST))
        strlcat(buf1, " ^L(BLACKLISTED)^N", sizeof(buf1));
      if (PLR_FLAGGED(tch, PLR_WANTED))
        strlcat(buf1, " ^R(WANTED)^N", sizeof(buf1));
      /*
      if (level >= LVL_VICEPRES && tch->char_specials.timer > 10)
        snprintf(ENDOF(buf1), sizeof(buf1) - strlen(buf1), " (idle: %d)", tch->char_specials.timer);
      */
      strlcat(buf1, "\r\n", sizeof(buf1));
      strlcat(buf, buf1, sizeof(buf));

      CGLOB = KNRM;
    }
  }

  if (num_can_see == 0) {
    snprintf(buf2, sizeof(buf2), "%s\r\nNo-one at all!\r\n", buf);
  } else if (num_can_see == 1) {
    if (num_in_socialization_rooms > 0) {
      snprintf(buf2, sizeof(buf2), "%s\r\nOne lonely chummer displayed and listed in ##^WWHERE^n. Might be time to start posting in /r/MUD?\r\n", buf);
    } else {
      snprintf(buf2, sizeof(buf2), "%s\r\nOne lonely chummer displayed... thank you for keeping the faith.\r\n", buf);
    }
  } else {
    snprintf(buf2, sizeof(buf2), "%s\r\n%d chummers displayed%s",
               buf,
               num_can_see,
               num_can_see == 69 ? " (nice)" : "");

    // We only show unique connections at 50+ PCs to ensure that folks have a harder time correlating alts.
    // Disabled this due to concerns from players.
#ifdef SHOW_UNIQUE_CONNECTION_COUNT
    if (num_can_see >= 50) {
      // We also drop between 0-2 displayed connections to make alt correlation harder.
      size_t displayed_size = (unique_connections.size() / 3) * 3;
      snprintf(ENDOF(buf2), sizeof(buf2) - strlen(buf2), " across ~%ld unique connection%s",
               displayed_size,
               displayed_size == 1 ? "" : "s");
    }
#endif

    if (num_in_socialization_rooms > 0) {
      snprintf(ENDOF(buf2), sizeof(buf2) - strlen(buf2), ", with %d listed in ##^WWHERE^n",
               num_in_socialization_rooms);
    }

    strlcat(buf2, ".\r\n", sizeof(buf2));
  }

  if (subcmd) {
    convert_and_write_string_to_file(buf2, "text/wholist");
    write_gsgp_file(num_can_see, "text/gsgp");
    MSSPSetPlayers(num_can_see);
  } else send_to_char(buf2, ch);
}

#define USERS_FORMAT "format: users [-n name] [-h host] [-o] [-p] [-g]\r\n"

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
  int outlaws = 0, playing = 0, deadweight = 0, globbed = 0;

  host_search[0] = name_search[0] = '\0';

  std::unordered_map< std::string, std::vector<std::string> > host_map = {};

#ifndef IS_BUILDPORT
  mudlog_vfprintf(ch, LOG_WIZLOG, "%s looking up user list with args '%s'", GET_CHAR_NAME(ch), argument);
#endif

  strlcpy(buf, argument, sizeof(buf));
  while (*buf) {
    half_chop(buf, arg, buf1, sizeof(buf1));
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
          half_chop(buf1, name_search, buf, sizeof(buf));
          break;
        case 'h':
          playing = 1;
          half_chop(buf1, host_search, buf, sizeof(buf));
          break;
        case 'g':
          if (!access_level(ch, LVL_PRESIDENT)) {
            send_to_char("Sorry, this mode is for game owners only.\r\n", ch);
            return;
          }
          globbed = 1;
          strlcpy(buf, buf1, sizeof(buf));
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

  if (globbed) {
    send_to_char(ch, "Host character counts:\r\n");
  } else {
    strlcpy(line, "Num  Name           State           Idle  Login@   Site\r\n", sizeof(line));
    strlcat(line, "---- -------------- --------------- ----- -------- ---------------------------\r\n", sizeof(line));
    send_to_char(line, ch);
  }

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
      if (*name_search && !isname(name_search, GET_KEYWORDS(tch)))
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
      snprintf(idletime, sizeof(idletime), "%5d", d->character->char_specials.timer);
    else
      strlcpy(idletime, "", sizeof(idletime));

    format = "%-4d %-14s %-15s %-5s %-8s ";

    if (d->character && GET_CHAR_NAME(d->character)) {
      if (d->original)
        snprintf(line, sizeof(line), // Flawfinder: ignore
                 format,
                 d->desc_num, GET_CHAR_NAME(d->original), state, idletime, timeptr);
      else
        snprintf(line, sizeof(line), // Flawfinder: ignore
                 format,
                 d->desc_num, GET_CHAR_NAME(d->character), state, idletime, timeptr);
    } else
      snprintf(line, sizeof(line), // Flawfinder: ignore
               format,
               d->desc_num, "UNDEFINED", state, idletime, timeptr);

    if (*d->host && GET_DESC_LEVEL(d) <= GET_LEVEL(ch))
      snprintf(ENDOF(line), sizeof(line) - strlen(line), "[%s]\r\n", d->host);
    else
      snprintf(ENDOF(line), sizeof(line) - strlen(line), "[hostname masked]\r\n");

    if (d->connected) {
      snprintf(line2, sizeof(line2), "^g%s^n", line);
      strlcpy(line, line2, sizeof(line));
    }
    if ((d->connected && !d->character) || CAN_SEE(ch, d->character)) {
      if (globbed) {
        try {
          host_map[std::string(d->host)].push_back(std::string(GET_CHAR_NAME(d->original ? d->original : d->character)));
        } catch (std::out_of_range) {
          host_map[std::string(d->host)] = std::vector<std::string>();
          host_map[std::string(d->host)].push_back(std::string(GET_CHAR_NAME(d->original ? d->original : d->character)));
        }
      } else {
        send_to_char(line, ch);
      }
      num_can_see++;
    }
  }

  if (globbed) {
    int total_hosts = 0;
    for (auto &host_it : host_map) {
      total_hosts++;
      send_to_char(ch, "^c%s^n: ", host_it.first.c_str());

      int total_pcs = 0;
      for (auto &pc_it : host_it.second) {
        send_to_char(ch, "%s%s", total_pcs++ > 0 ? ", " : "", pc_it.c_str());
      }
      send_to_char(ch, " (^c%d^n PC%s)\r\n", total_pcs, total_pcs == 1 ? "" : "s");
    }
    send_to_char(ch, "\r\n%d visible host%s connected.\r\n", total_hosts, total_hosts == 1 ? "" : "s");
  } else {
    send_to_char(ch, "\r\n%d visible socket%s connected.\r\n", num_can_see, num_can_see == 1 ? "" : "s");
  }
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
      // page_string(ch->desc, info, 0);
      do_help(ch, argument, 0, 0);
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

bool ch_can_see_vict_in_where(struct char_data *ch, struct char_data *vict) {
  // Skip them if they aren't in a social-bonus room.
  if (!vict || !char_is_in_social_room(vict))
    return FALSE;

  // Ignore disconnected folks and those who are in the middle of writing something etc.
  if (!vict->desc || vict->desc->connected)
    return FALSE;

  // Skip them if you can't see them for various reasons.
  if (IS_IGNORING(vict, is_blocking_where_visibility_for, ch) || !CAN_SEE(ch, vict))
    return FALSE;  

  // Don't count staff in social rooms.
  if (IS_SENATOR(vict))
    return FALSE;

  // Skip them if they've not acted in the last 30 minutes or are flagged AFK. 
  if (vict->char_specials.timer >= 30 || PRF_FLAGGED(vict, PRF_AFK))
    return FALSE;

  return TRUE;
}

void perform_mortal_where(struct char_data * ch, char *arg)
{
  // Locating belongings.
  if (*arg && (is_abbrev(arg, "belongings") || is_abbrev(arg, "corpses") || is_abbrev(arg, "stuff") || is_abbrev(arg, "body") || is_abbrev(arg, "bodies"))) {
    // Clear buf first so we don't send junk:
    strlcpy(buf, "", sizeof(buf));

    // Query the object list.
    if (ObjList.PrintBelongings(ch) == 0) {
      send_to_char("You query DocWagon's list of places they couldn't retrieve your belongings from, but nothing comes up.\r\n", ch);
    } else {
      send_to_char(ch, "You take a moment and query DocWagon's list of places they couldn't retrieve your belongings from:\r\n%s", buf);
    }
    return;
  }

  // array slot 0 is total PCs, array slot 1 is active PCs
  std::unordered_map<vnum_t, std::vector<struct char_data *>> occupied_rooms = {};
  std::unordered_map<vnum_t, std::vector<struct char_data *>>::iterator room_iterator;

  for (struct descriptor_data *d = descriptor_list; d; d = d->next) {
    struct char_data *i = (d->original ? d->original : d->character);

    if (!ch_can_see_vict_in_where(ch, i))
      continue;

    // They're a valid target-- emplace them.
    if ((room_iterator = occupied_rooms.find(GET_ROOM_VNUM(i->in_room))) != occupied_rooms.end()) {
      (room_iterator->second).push_back(i);
    } else {
      std::vector<struct char_data *> tmp_vec = { i };
      occupied_rooms.emplace(GET_ROOM_VNUM(i->in_room), tmp_vec);
    }
  }

  if (occupied_rooms.empty()) {
    send_to_char("Nobody's active in a socialization room right now. Why not go start something?\r\n", ch);
    return;
  } else {
    send_to_char("There are people RPing in these rooms:\r\n", ch);
    for (auto it = occupied_rooms.begin(); it != occupied_rooms.end(); ++it) {
      bool printed_something = FALSE;
      int num_masked_people = 0;

      send_to_char(ch, "%s^n (", world[real_room(it->first)].name);

      for (size_t i = 0; i < (it->second).size(); i++) {
        if (PRF_FLAGGED((it->second)[i], PRF_ANONYMOUS_ON_WHERE)) {
          num_masked_people++;
        } else {
          send_to_char(ch, "%s%s", printed_something ? ", " : "", GET_CHAR_NAME((it->second)[i]));
          printed_something = TRUE;
        }
      }

      if (num_masked_people > 0) {
        send_to_char(ch, "%s%d anonymous character%s", printed_something ? " and " : "", num_masked_people, num_masked_people > 1 ? "s" : "");
      }

      send_to_char(")\r\n", ch);
    }

    if (char_is_in_social_room(ch)) {
      GET_PLAYER_WHERE_COMMANDS(ch) = 0;
    } else if ((++GET_PLAYER_WHERE_COMMANDS(ch)) % 5 == 0) {
      send_to_char(ch, "(OOC: You've been checking the wherelist a lot-- why not go join in?)\r\n");
    }
  }
}

void print_object_location(int num, struct obj_data *obj, struct char_data *ch,
                           int recur)
{
  if (num > 0)
    snprintf(ENDOF(buf), sizeof(buf) - strlen(buf), "O%3d. %-25s - ", num,
            GET_OBJ_NAME(obj));
  else
    snprintf(ENDOF(buf), sizeof(buf) - strlen(buf), "%33s", " - ");

  if (obj->in_room)
    snprintf(ENDOF(buf), sizeof(buf) - strlen(buf), "[%5ld] %s^n", GET_ROOM_VNUM(obj->in_room), GET_ROOM_NAME(obj->in_room));
  else if (obj->carried_by) {
    snprintf(ENDOF(buf), sizeof(buf) - strlen(buf), "carried by %s^n", GET_CHAR_NAME(obj->carried_by));
    if (obj->carried_by->in_room) {
      snprintf(ENDOF(buf), sizeof(buf) - strlen(buf), " @ room %ld (%s^n)", GET_ROOM_VNUM(obj->carried_by->in_room), GET_ROOM_NAME(obj->carried_by->in_room));
    } else if (obj->carried_by->in_veh) {
      snprintf(ENDOF(buf), sizeof(buf) - strlen(buf), " @ veh %ld (%s^n)", GET_VEH_VNUM(obj->carried_by->in_veh), GET_VEH_NAME(obj->carried_by->in_veh));
    } else {
      strlcat(buf, " ^Rnowhere^n", sizeof(buf));
    }
  }
  else if (obj->worn_by) {
    snprintf(ENDOF(buf), sizeof(buf) - strlen(buf), "worn by %s^n", GET_CHAR_NAME(obj->worn_by));
    if (obj->worn_by->in_room) {
      snprintf(ENDOF(buf), sizeof(buf) - strlen(buf), " @ room %ld (%s^n)", GET_ROOM_VNUM(obj->worn_by->in_room), GET_ROOM_NAME(obj->worn_by->in_room));
    } else if (obj->worn_by->in_veh) {
      snprintf(ENDOF(buf), sizeof(buf) - strlen(buf), " @ veh %ld (%s^n)", GET_VEH_VNUM(obj->worn_by->in_veh), GET_VEH_NAME(obj->worn_by->in_veh));
    } else {
      strlcat(buf, " ^Rnowhere^n", sizeof(buf));
    }
  } else if (obj->in_obj) {
    snprintf(ENDOF(buf), sizeof(buf) - strlen(buf), "inside %s%s^n",
            GET_OBJ_NAME(obj->in_obj),
            (recur ? ", which is\r\n" : " "));
    if (recur)
      print_object_location(0, obj->in_obj, ch, recur);
  } else if (obj->in_veh) {
    snprintf(ENDOF(buf), sizeof(buf) - strlen(buf), "in %s @ %ld%s^n", GET_VEH_NAME(obj->in_veh), GET_ROOM_VNUM(get_obj_in_room(obj)), obj->in_veh->in_veh ? " (nested veh)" : "");
  } else if (obj->in_host) {
    snprintf(ENDOF(buf), sizeof(buf) - strlen(buf), "in host %s (%ld)^n", obj->in_host->name, obj->in_host->vnum);
  } else
    strlcat(buf, " in an unknown location.", sizeof(buf));

  strlcat(buf, "\r\n", sizeof(buf));
}

void perform_immort_where(struct char_data * ch, char *arg)
{
  struct char_data *primary_char;
  struct descriptor_data *d;
  struct room_data *room;
  int num = 0, found = 0;
  int found2 = FALSE;

  if (!*arg)
  {
#ifndef IS_BUILDPORT
    mudlog_vfprintf(ch, LOG_WIZLOG, "%s queried locations of all players.", GET_CHAR_NAME(ch));
#endif
    strlcpy(buf, "Players\r\n-------\r\n", sizeof(buf));
    for (d = descriptor_list; d; d = d->next)
      if (!d->connected) {
        // Always points to the main body.
        primary_char = (d->original ? d->original : d->character);

        if (primary_char && CAN_SEE(ch, primary_char) && (primary_char->in_room || primary_char->in_veh)) {
          // The room of the character they're currently inhabiting- either main body or projection.
          room = get_ch_in_room(d->character);

          snprintf(buf1, sizeof(buf1), "%-20s - [%6ld] %s^n%s",
                    GET_CHAR_NAME(primary_char),
                    GET_ROOM_VNUM(room),
                    ch_can_bypass_edit_lock(ch, room) ? GET_ROOM_NAME(room) : "<redacted>",
                    ROOM_FLAGGED(room, ROOM_ENCOURAGE_CONGREGATION) ? " ^c(social room)^n" : ""
                  );

          if (d->character->in_veh) {
            snprintf(ENDOF(buf1), sizeof(buf1), " (in %s^n)", GET_VEH_NAME(d->character->in_veh));
          }

          if (d->original) {
            snprintf(ENDOF(buf1), sizeof(buf1), " (switched as %s^n)", GET_NAME(d->character));
          }

          strlcat(buf, buf1, sizeof(buf));
          strlcat(buf, "\r\n", sizeof(buf));
        }
      }
    page_string(ch->desc, buf, 1);
    return;
  }

#ifndef IS_BUILDPORT
  mudlog_vfprintf(ch, LOG_WIZLOG, "%s queried locations of anything with keyword '%s'.", GET_CHAR_NAME(ch), arg);
#endif

  // Location version of the command (where <keyword>)
  *buf = '\0';
  for (struct char_data *i = character_list; i; i = i->next_in_character_list)
    if ((i->in_room || i->in_veh) && CAN_SEE(ch, i) && keyword_appears_in_char(arg, i, TRUE, TRUE, FALSE)) {
      found = 1;
      room = get_ch_in_room(i);
      snprintf(ENDOF(buf), sizeof(buf) - strlen(buf), "M%3d. %-25s - [%5ld] %s^n", ++num,
                GET_NAME(i),
                GET_ROOM_VNUM(room),
                GET_ROOM_NAME(room)
              );
      if (i->in_veh) {
        snprintf(ENDOF(buf), sizeof(buf) - strlen(buf), " (in %s^n)\r\n", GET_VEH_NAME(i->in_veh));
      } else {
        strlcat(buf, "\r\n", sizeof(buf));
      }
    }
  found2 = ObjList.PrintList(ch, arg, TRUE);

  if (!found && !found2)
    send_to_char("Couldn't find any such thing.\r\n", ch);
  else
    page_string(ch->desc, buf, 1);
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

  one_argument(argument, buf);

  if (!(victim = get_char_room_vis(ch, buf))) {
    send_to_char("Consider killing who?\r\n", ch);
    return;
  }
  if (victim == ch) {
    send_to_char("You can't target yourself with this command.\r\n", ch);
    return;
  }

  if (IS_NPC(victim)) {
    if (!can_hurt(ch, victim, 0, TRUE)) {
      act("You can't damage $N.", FALSE, ch, 0, victim, TO_CHAR);
      return;
    }

    snprintf(buf3, sizeof(buf3), "You look $N over. %s %s:", CAP(HSSH(victim)), HSSH_SHOULD_PLURAL(victim) ? "is" : "are");
    act(buf3, FALSE, ch, 0, victim, TO_CHAR);

    // Pick out the victim's cyberware, if any. TODO: Player cyberware.
    bool victim_uses_cyber_implants = FALSE, ch_uses_cyber_implants = FALSE;
    int vict_unarmed_dangerliciousness_boost = 0, ch_unarmed_dangerliciousness_boost = 0;
    for (struct obj_data *obj = victim->cyberware; obj; obj = obj->next_content) {
      if (!GET_CYBERWARE_IS_DISABLED(obj)) {
        switch (GET_CYBERWARE_TYPE(obj)) {
          case CYB_CLIMBINGCLAWS:
          case CYB_FIN:
          case CYB_HANDBLADE:
          case CYB_HANDRAZOR:
          case CYB_HANDSPUR:
          case CYB_FOOTANCHOR:
            victim_uses_cyber_implants = TRUE;
            break;
        }
      } else if (GET_CYBERWARE_TYPE(obj) == CYB_BONELACING) {
        switch (GET_CYBERWARE_LACING_TYPE(obj)) {
          case BONE_PLASTIC:
            vict_unarmed_dangerliciousness_boost = MAX(vict_unarmed_dangerliciousness_boost, 2);
            break;
          case BONE_ALUMINIUM:
          case BONE_CERAMIC:
            vict_unarmed_dangerliciousness_boost = MAX(vict_unarmed_dangerliciousness_boost, 3);
            break;
          case BONE_TITANIUM:
            vict_unarmed_dangerliciousness_boost = MAX(vict_unarmed_dangerliciousness_boost, 4);
            break;
        }
      }
    }
    for (struct obj_data *obj = ch->cyberware; obj; obj = obj->next_content) {
      if (!GET_CYBERWARE_IS_DISABLED(obj)) {
        switch (GET_CYBERWARE_TYPE(obj)) {
          case CYB_CLIMBINGCLAWS:
          case CYB_FIN:
          case CYB_HANDBLADE:
          case CYB_HANDRAZOR:
          case CYB_HANDSPUR:
          case CYB_FOOTANCHOR:
            ch_uses_cyber_implants = TRUE;
            break;
        }
      } else if (GET_CYBERWARE_TYPE(obj) == CYB_BONELACING) {
        switch (GET_CYBERWARE_LACING_TYPE(obj)) {
          case BONE_PLASTIC:
            ch_unarmed_dangerliciousness_boost = MAX(ch_unarmed_dangerliciousness_boost, 2);
            break;
          case BONE_ALUMINIUM:
          case BONE_CERAMIC:
            ch_unarmed_dangerliciousness_boost = MAX(ch_unarmed_dangerliciousness_boost, 3);
            break;
          case BONE_TITANIUM:
            ch_unarmed_dangerliciousness_boost = MAX(ch_unarmed_dangerliciousness_boost, 4);
            break;
        }
      }
    }

    // Armor comparisons.
    int armor_diff = (GET_BALLISTIC(victim) - GET_BALLISTIC(ch)) + (GET_IMPACT(victim) - GET_IMPACT(ch));
    if (armor_diff < -8) {
      act("- much less protected than you are.", FALSE, ch, 0, victim, TO_CHAR);
    } else if (armor_diff < -6) {
      act("- less protected than you are.", FALSE, ch, 0, victim, TO_CHAR);
    } else if (armor_diff < -4) {
      act("- somewhat less protected than you are.", FALSE, ch, 0, victim, TO_CHAR);
    } else if (armor_diff < -2) {
      act("- a tiny bit less protected than you are.", FALSE, ch, 0, victim, TO_CHAR);
    } else if (armor_diff < 2) {
      act("- about as protected as you are.", FALSE, ch, 0, victim, TO_CHAR);
    } else if (armor_diff < 4) {
      act("- a tiny bit more protected than you are.", FALSE, ch, 0, victim, TO_CHAR);
    } else if (armor_diff < 6) {
      act("- somewhat more protected than you are.", FALSE, ch, 0, victim, TO_CHAR);
    } else if (armor_diff < 8) {
      act("- more protected than you are.", FALSE, ch, 0, victim, TO_CHAR);
    } else {
      act("- much more protected than you are.", FALSE, ch, 0, victim, TO_CHAR);
    }

    int expected_power;
    struct obj_data *weapon = GET_EQ(ch, WEAR_WIELD);
    if (!weapon || GET_OBJ_TYPE(weapon) != ITEM_WEAPON)
      weapon = GET_EQ(ch, WEAR_HOLD);
    if (!weapon || GET_OBJ_TYPE(weapon) != ITEM_WEAPON)
      weapon = NULL;
    if (weapon) {
      if (WEAPON_IS_GUN(weapon)) {
        expected_power = GET_WEAPON_POWER(weapon);

        if (GET_WEAPON_ATTACK_TYPE(weapon) == WEAP_TASER) {
          expected_power -= (int)(GET_IMPACT(victim) / 2);
        } else if (weapon->contains) {
          switch (GET_MAGAZINE_AMMO_TYPE(weapon->contains)) {
            case AMMO_APDS:
            case AMMO_AV:
              expected_power -= (int)(GET_BALLISTIC(victim) / 2);
              break;
            case AMMO_EX:
              expected_power++;
              // fall through
            case AMMO_EXPLOSIVE:
              expected_power += 1 - GET_BALLISTIC(victim);
              break;
            case AMMO_FLECHETTE:
              expected_power += 1 - MAX(GET_BALLISTIC(victim), GET_IMPACT(victim) * 2);
              break;
            case AMMO_HARMLESS:
              expected_power = 0;
              break;
            case AMMO_GEL:
              expected_power -= GET_IMPACT(victim) - 2;
              break;
            default:
              expected_power = GET_WEAPON_POWER(weapon) - GET_BALLISTIC(victim);
              break;
          }
        } else {
          expected_power = GET_WEAPON_POWER(weapon) - GET_BALLISTIC(victim);
        }
      }
      else {
        expected_power = GET_WEAPON_STR_BONUS(weapon) + GET_STR(ch) - GET_IMPACT(victim);
      }
    } else {
      // should probably account for cyber implant weapons here... eventual todo
      expected_power = GET_STR(ch) + ch_unarmed_dangerliciousness_boost - GET_IMPACT(victim);
    }

    if (IS_SPIRIT(victim) || IS_ANY_ELEMENTAL(victim)) {
      bool has_magic_weapon;

      if (weapon) {
        has_magic_weapon = GET_WEAPON_FOCUS_RATING(weapon) > 0 && is_weapon_focus_usable_by(weapon, ch);
      } else {
        has_magic_weapon = GET_POWER(ch, ADEPT_KILLING_HANDS);
      }

      // If you're not using a magic weapon, the spirit uses its Immunity to Normal Weapons power.
      if (!has_magic_weapon) {
        // Extremely hacky version of spirit resists.
        expected_power -= (GET_LEVEL(victim) * 2) + 1;
        act("- a magical being, so dealing any damage will be difficult.", FALSE, ch, 0, victim, TO_CHAR);
      }
    }

    int vict_expected_power;
    struct obj_data *vict_weapon = GET_EQ(victim, WEAR_WIELD);
    if (!vict_weapon || GET_OBJ_TYPE(vict_weapon) != ITEM_WEAPON)
      vict_weapon = GET_EQ(victim, WEAR_HOLD);
    if (!vict_weapon || GET_OBJ_TYPE(vict_weapon) != ITEM_WEAPON)
      vict_weapon = NULL;
    if (vict_weapon) {
      if (WEAPON_IS_GUN(vict_weapon)) {
        vict_expected_power = GET_WEAPON_POWER(vict_weapon);

        if (GET_WEAPON_ATTACK_TYPE(vict_weapon) == WEAP_TASER) {
          vict_expected_power -= (int)(GET_IMPACT(ch) / 2);
        } else if (vict_weapon->contains) {
          switch (GET_MAGAZINE_AMMO_TYPE(vict_weapon->contains)) {
            case AMMO_APDS:
            case AMMO_AV:
              vict_expected_power -= (int)(GET_BALLISTIC(ch) / 2);
              break;
            case AMMO_EX:
              vict_expected_power++;
              // fall through
            case AMMO_EXPLOSIVE:
              vict_expected_power += 1 - GET_BALLISTIC(ch);
              break;
            case AMMO_FLECHETTE:
              vict_expected_power += 1 - MAX(GET_BALLISTIC(ch), GET_IMPACT(ch) * 2);
              break;
            case AMMO_HARMLESS:
              vict_expected_power = 0;
              break;
            case AMMO_GEL:
              vict_expected_power -= GET_IMPACT(ch) - 2;
              break;
            default:
              vict_expected_power -= GET_BALLISTIC(ch);
              break;
          }
        } else {
          vict_expected_power = GET_WEAPON_POWER(vict_weapon) - GET_BALLISTIC(ch);
        }
      }
      else {
        vict_expected_power = GET_WEAPON_STR_BONUS(vict_weapon) + GET_STR(victim) - GET_IMPACT(ch);
      }
    } else {
      // should probably account for cyber implant weapons here... eventual todo
      vict_expected_power = GET_STR(victim) + vict_unarmed_dangerliciousness_boost - GET_IMPACT(ch);
    }

    // Stat comparisons.
    int phys_stat_diff = (GET_BOD(victim) - GET_BOD(ch)) + (GET_QUI(victim) - GET_QUI(ch)) + (GET_STR(victim) - GET_STR(ch));
    if (phys_stat_diff < -12) {
      act("- much less physically developed than you are.", FALSE, ch, 0, victim, TO_CHAR);
    } else if (phys_stat_diff < -9) {
      act("- less physically developed than you are.", FALSE, ch, 0, victim, TO_CHAR);
    } else if (phys_stat_diff < -6) {
      act("- somewhat less physically developed than you are.", FALSE, ch, 0, victim, TO_CHAR);
    } else if (phys_stat_diff < -3) {
      act("- a tiny bit less physically developed than you are.", FALSE, ch, 0, victim, TO_CHAR);
    } else if (phys_stat_diff < 3) {
      act("- about as physically developed as you are.", FALSE, ch, 0, victim, TO_CHAR);
    } else if (phys_stat_diff < 6) {
      act("- a tiny bit more physically developed than you are.", FALSE, ch, 0, victim, TO_CHAR);
    } else if (phys_stat_diff < 9) {
      act("- somewhat more physically developed than you are.", FALSE, ch, 0, victim, TO_CHAR);
    } else if (phys_stat_diff < 12) {
      act("- more physically developed than you are.", FALSE, ch, 0, victim, TO_CHAR);
    } else {
      act("- much more physically developed than you are.", FALSE, ch, 0, victim, TO_CHAR);
    }

    // Extra init passes are king, so account for that.
    int speed_diff = (GET_REA(victim) - GET_REA(ch)) + (4.2 * (GET_INIT_DICE(victim) - GET_INIT_DICE(ch)));
    if (speed_diff < -20) {
      act("- much slower than you are.", FALSE, ch, 0, victim, TO_CHAR);
    } else if (speed_diff < -15) {
      act("- slower than you are.", FALSE, ch, 0, victim, TO_CHAR);
    } else if (speed_diff < -10) {
      act("- somewhat slower than you are.", FALSE, ch, 0, victim, TO_CHAR);
    } else if (speed_diff < -5) {
      act("- a tiny bit slower than you are.", FALSE, ch, 0, victim, TO_CHAR);
    } else if (speed_diff < 5) {
      act("- about as fast as you are.", FALSE, ch, 0, victim, TO_CHAR);
    } else if (speed_diff < 10) {
      act("- a tiny bit faster than you are.", FALSE, ch, 0, victim, TO_CHAR);
    } else if (speed_diff < 15) {
      act("- somewhat faster than you are.", FALSE, ch, 0, victim, TO_CHAR);
    } else if (speed_diff < 20) {
      act("- faster than you are.", FALSE, ch, 0, victim, TO_CHAR);
    } else {
      act("- much faster than you are.", FALSE, ch, 0, victim, TO_CHAR);
    }

    // Skill / Pool comparisons.
    int combat_skill_diff = (GET_COMBAT_POOL(victim) - GET_COMBAT_POOL(ch));

    if (GET_EQ(victim, WEAR_WIELD)) {
      combat_skill_diff += MAX(GET_SKILL(victim, GET_WEAPON_SKILL(GET_EQ(victim, WEAR_WIELD))), GET_SKILL(victim, SKILL_ARMED_COMBAT));
    } else if (victim_uses_cyber_implants) {
      combat_skill_diff += MAX(GET_SKILL(victim, SKILL_CYBER_IMPLANTS), GET_SKILL(victim, SKILL_ARMED_COMBAT));
    } else {
      combat_skill_diff += GET_SKILL(victim, SKILL_UNARMED_COMBAT) + vict_unarmed_dangerliciousness_boost;
    }

    if (GET_EQ(ch, WEAR_WIELD)) {
      combat_skill_diff -= GET_SKILL(ch, GET_WEAPON_SKILL(GET_EQ(ch, WEAR_WIELD)));
    } else if (ch_uses_cyber_implants) {
      combat_skill_diff += GET_SKILL(ch, SKILL_CYBER_IMPLANTS);
    } else {
      combat_skill_diff -= GET_SKILL(ch, SKILL_UNARMED_COMBAT) + ch_unarmed_dangerliciousness_boost;
    }

    if (combat_skill_diff < -10) {
      act("- much less skilled than you are.", FALSE, ch, 0, victim, TO_CHAR);
    } else if (combat_skill_diff < -7) {
      act("- less skilled than you are.", FALSE, ch, 0, victim, TO_CHAR);
    } else if (combat_skill_diff < -4) {
      act("- somewhat less skilled than you are.", FALSE, ch, 0, victim, TO_CHAR);
    } else if (combat_skill_diff < -1) {
      act("- a tiny bit less skilled than you are.", FALSE, ch, 0, victim, TO_CHAR);
    } else if (combat_skill_diff < 1) {
      act("- about as skilled as you are.", FALSE, ch, 0, victim, TO_CHAR);
    } else if (combat_skill_diff < 4) {
      act("- a tiny bit more skilled than you are.", FALSE, ch, 0, victim, TO_CHAR);
    } else if (combat_skill_diff < 7) {
      act("- somewhat more skilled than you are.", FALSE, ch, 0, victim, TO_CHAR);
    } else if (combat_skill_diff < 10) {
      act("- more skilled than you are.", FALSE, ch, 0, victim, TO_CHAR);
    } else {
      act("- much more skilled than you are.", FALSE, ch, 0, victim, TO_CHAR);
    }

    if (expected_power <= 0) {
      act("You'd need a lot of luck for your weapon to even scratch $M...", FALSE, ch, 0, victim, TO_CHAR);
    } else if (expected_power <= 3) {
      act("You feel like your weapon might be able to put a scratch on $M...", FALSE, ch, 0, victim, TO_CHAR);
    } else if (expected_power <= 6) {
      act("You feel like your weapon would be able to annoy $M at least.", FALSE, ch, 0, victim, TO_CHAR);
    } else {
      act("You feel like your weapon should be able to do some damage to $M.", FALSE, ch, 0, victim, TO_CHAR);
    }

    if (vict_expected_power <= 0) {
      act("You don't sense much danger from $M.", FALSE, ch, 0, victim, TO_CHAR);
    } else if (vict_expected_power <= 3) {
      act("$E could probably scratch you...", FALSE, ch, 0, victim, TO_CHAR);
    } else if (vict_expected_power <= 6) {
      act("$E would be annoying to fight.", FALSE, ch, 0, victim, TO_CHAR);
    } else {
      act("You're pretty sure $E could hurt you.", FALSE, ch, 0, victim, TO_CHAR);
    }
  } else {
    if (GET_REP(victim) < NEWBIE_KARMA_THRESHOLD)
      send_to_char("Total greenhorn.\r\n", ch);
    else if (GET_REP(victim) < 200)
      send_to_char("Still finding their feet.\r\n", ch);
    else if (GET_REP(victim) < 400)
      send_to_char("Innocence has been lost.\r\n", ch);
    else if (GET_REP(victim) < 800)
      send_to_char("They can handle themselves.\r\n", ch);
    else if (GET_REP(victim) < 1600)
      send_to_char("An accomplished runner.\r\n", ch);
    else if (GET_REP(victim) < 3200)
      send_to_char("Definite lifer.\r\n", ch);
    else if (GET_REP(victim) < 10000)
      send_to_char("A legend of the Sprawl.\r\n", ch);
    else // 10k+ rep, the people who have been playing for ages.
      send_to_char("Could probably be trusted with package deliveries.\r\n", ch);
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

      // Skip any unsupported commands.
      /*
      if (mtx_info[cmd_num].command_pointer == do_unsupported_command)
        continue;
      */

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
  } else if (IS_RIGGING(ch)) {
    for (no = 1, cmd_num = 1; *rig_info[cmd_num].command != '\n';cmd_num++) {
      // Skip any commands that don't match the prefix provided.
      if (!mode_all && *arg && !is_abbrev(arg, rig_info[cmd_num].command))
        continue;

      // Skip any unsupported commands.
      /*
      if (rig_info[cmd_num].command_pointer == do_unsupported_command)
        continue;
      */

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

      // TODO: Skip any unsupported commands.
      /*
      if ((cmd_info[cmd_num].command_pointer) == &do_unsupported_command)
        continue;
      */

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

bool char_passed_moving_vehicle_perception_test(struct char_data *ch, struct veh_data *in_veh) {
  if (!in_veh || in_veh->cspeed <= SPEED_IDLE)
    return TRUE;

  if (get_speed(in_veh) >= 200) {
    return (success_test(GET_INT(ch) + GET_POWER(ch, ADEPT_IMPROVED_PERCEPT), 7) >= 1);
  }
  else if (get_speed(in_veh) < 200 && get_speed(in_veh) >= 120) {
    return (success_test(GET_INT(ch) + GET_POWER(ch, ADEPT_IMPROVED_PERCEPT), 6) >= 1);
  }
  else if (get_speed(in_veh) < 120 && get_speed(in_veh) >= 60) {
    return (success_test(GET_INT(ch) + GET_POWER(ch, ADEPT_IMPROVED_PERCEPT), 5) >= 1);
  }

  return (success_test(GET_INT(ch) + GET_POWER(ch, ADEPT_IMPROVED_PERCEPT), 4) >= 1);
}

const char *render_room_for_scan(struct char_data *ch, struct room_data *room, struct veh_data *in_veh) {
  static char results[10000];
  results[0] = '\0';

  // If you can't see into the room, skip.
  if (!LIGHT_OK_ROOM_SPECIFIED(ch, room)) {
    DEBUG_TO_STAFF(ch, "bad light\r\n");
    return NULL;
  }

  // TODO: Add logic to hide ultrasound-visible-only characters in adjacent silent rooms. Not a big urgency unless/until PvP becomes more of a thing.

  bool ch_only_sees_with_astral = !LIGHT_OK_ROOM_SPECIFIED(ch, room, FALSE);

  // First, people.
  for (struct char_data *list = room->people; list; list = list->next_in_room) {
    // Always skip invisible people.
    if (!CAN_SEE(ch, list)) {
      DEBUG_TO_STAFF(ch, "%s: can't see\r\n", GET_CHAR_NAME(list));
      continue;
    }

    if (IS_IGNORING(ch, is_blocking_ic_interaction_from, list)) {
      DEBUG_TO_STAFF(ch, "%s: scanner is ignoring\r\n", GET_CHAR_NAME(list));
      continue;
    }

    // Skip inanimates if you only see with astral.
    if (ch_only_sees_with_astral && MOB_FLAGGED(list, MOB_INANIMATE)) {
      DEBUG_TO_STAFF(ch, "%s: astral-only vs inanimate\r\n", GET_CHAR_NAME(list));
      continue;
    }

    // Moving vehicles have a chance to cause you to miss things on scan.
    if (!char_passed_moving_vehicle_perception_test(ch, in_veh)) {
      DEBUG_TO_STAFF(ch, "%s: failed moving vehicle perception test\r\n", GET_CHAR_NAME(list));
      continue;
    }

    // You see the person-- render them.
    {
      // First, fill out the prepended flags.
      char flag_line[512] = { '\0' };

      if (GET_MOB_QUEST_CHAR_ID(list)) {
        if (GET_MOB_QUEST_CHAR_ID(list) == GET_IDNUM_EVEN_IF_PROJECTING(ch)) {
          strlcat(flag_line, "(quest) ", sizeof(flag_line));
        } else if (!ch_is_blocked_by_quest_protections(ch, list)) {
          strlcat(flag_line, "(group quest) ", sizeof(flag_line));
        } else {
          strlcat(flag_line, "(protected) ", sizeof(flag_line));
        }
      }

      if (IS_AFFECTED(list, AFF_RUTHENIUM) || IS_AFFECTED(list, AFF_IMP_INVIS) || IS_AFFECTED(list, AFF_SPELLINVIS) || IS_AFFECTED(list, AFF_SPELLIMPINVIS)) {
        strlcat(flag_line, "(invisible) ", sizeof(flag_line));
      }

      if (SEES_ASTRAL(ch) && IS_ASTRAL(list)) {
          strlcat(flag_line, "(astral) ", sizeof(flag_line));
      }

      // Next, render their name, replacing it with their memorized name if you have that.
      char their_name[500];
      strlcpy(their_name, GET_NAME(list), sizeof(their_name));
      if (!IS_NPC(list)) {
        struct remem *mem = safe_found_mem(ch, list);
        if (mem)
          strlcpy(their_name, CAP(mem->mem), sizeof(their_name));
        else if (IS_SENATOR(ch))
          strlcpy(their_name, GET_CHAR_NAME(list), sizeof(their_name));
      }

      // Finally, append it to the output buf.
      snprintf(ENDOF(results), sizeof(results) - strlen(results), "  %s%s^n%s\r\n",
               flag_line,
               their_name,
               FIGHTING(list) == ch ? " ^R(fighting you!)^n" : (mob_is_alarmed_by_ch(list, ch) && (MOB_FLAGGED(list, MOB_HELPER) || MOB_FLAGGED(list, MOB_GUARD)) ? " ^r(alarmed)^n" : ""));
    }
  }

  // Next, vehicles.
  for (struct veh_data *veh = room->vehicles; veh; veh = veh->next_veh) {
    if (!char_passed_moving_vehicle_perception_test(ch, in_veh))
      continue;

    snprintf(ENDOF(results), sizeof(results) - strlen(results), "  %s^n (%s)\r\n", GET_VEH_NAME(veh), (get_speed(veh) ? "Moving" : "Stationary"));
  }

  // Player belongings.
  for (struct obj_data *obj = room->contents; obj; obj = obj->next_content) {
    if (GET_OBJ_VNUM(obj) == OBJ_SPECIAL_PC_CORPSE) {
      snprintf(ENDOF(results), sizeof(results) - strlen(results), "  %s^n\r\n", GET_OBJ_NAME(obj));
    }
  }

  return results;
}

ACMD(do_scan)
{
  bool specific = FALSE;
  int dir = 0;
  struct room_data *in_room = get_ch_in_room(ch);
  struct veh_data *in_veh = NULL;

  argument = any_one_arg(argument, buf);

  if (*buf) {
    if (is_abbrev(buf, "south")) {
      dir = SCMD_SOUTH; // why SCMD specifically? That's incremented by 1 compared to the raw directions
      specific = TRUE;
    } else {
      for (; !specific && (dir < NUM_OF_DIRS); ++dir) {
        if (is_abbrev(buf, dirs[dir]))
          specific = TRUE;
      }
    }
  }

  RIG_VEH(ch, in_veh);
  if (in_veh)
    in_room = get_veh_in_room(in_veh);

  // Scanning all rooms:
  if (!specific) {
    bool anythere = FALSE;

    for (int dir = 0; dir < NUM_OF_DIRS; ++dir) {
      if (!CAN_GO2(in_room, dir) || IS_SET(in_room->dir_option[dir]->exit_info, EX_HIDDEN))
        continue;

      if (in_room->dir_option[dir]->to_room == in_room) {
        send_to_char(ch, "%s: More of the same.\r\n", dirs[dir]);
        continue;
      }

      const char *result = render_room_for_scan(ch, in_room->dir_option[dir]->to_room, in_veh);

      if (result && *result) {
        anythere = TRUE;
        send_to_char(ch, "%s %s:\r\n%s\r\n", CAP(dirs[dir]), dist_name[0], result);
      }
    }

    if (!anythere) {
      send_to_char("You don't seem to see anyone in the surrounding areas.\r\n", ch);
      return;
    }
  }
  else {
    --dir;

    bool anythere = FALSE;

    for (int dist = 1; dist <= find_sight(ch); dist++) {
      if (!CAN_GO2(in_room, dir) || IS_SET(in_room->dir_option[dir]->exit_info, EX_HIDDEN)) {
        if (dist == 1)
          send_to_char("There is no exit in that direction.\r\n", ch);
        if (!anythere) {
          send_to_char("You don't seem to see anyone in that direction.\r\n", ch);
        }
        return;
      }

      in_room = in_room->dir_option[dir]->to_room;

      const char *result = render_room_for_scan(ch, in_room, in_veh);

      if (result && *result) {
        anythere = TRUE;
        send_to_char(ch, "%s %s:\r\n%s\r\n", CAP(dirs[dir]), dist_name[dist - 1], result);
      }
    }

    if (!anythere) {
      send_to_char("You don't seem to see anyone in that direction.\r\n", ch);
    }
  }
}


ACMD(do_position)
{
  struct veh_data *veh = get_veh_controlled_by_char(ch);

  skip_spaces(&argument);
  if (!*argument) {
    send_to_char("Your current position is:\r\n  ", ch);
    if (veh) {
      send_to_char(ch, "%s %s\r\n", capitalize(GET_VEH_NAME_NOFORMAT(veh)), GET_VEH_DEFPOS(veh));
    } else {
      list_one_char(ch, ch);
    }
    return;
  }

  if (is_abbrev(argument, "clear")) {
    if (veh) {
      DELETE_ARRAY_IF_EXTANT(GET_VEH_DEFPOS(veh));
    } else {
      DELETE_ARRAY_IF_EXTANT(GET_DEFPOS(ch));
    }
    send_to_char("Position cleared.\r\n", ch);
    return;
  }

  if (GET_POS(ch) == POS_FIGHTING) {
    send_to_char("You can't set your position while fighting.\r\n", ch);
    return;
  }

  if (veh && veh->cspeed > SPEED_IDLE) {
    send_to_char("You can only set a vehicle's position while it's idle.\r\n", ch);
    return;
  }

  // Auto-punctuate it.
  if (!ispunct(get_final_character_from_string(get_string_after_color_code_removal(argument, ch)))) {
    strlcat(argument, ".", sizeof(argument));
  }

  if (veh) {    
    DELETE_ARRAY_IF_EXTANT(GET_VEH_DEFPOS(veh));
    GET_VEH_DEFPOS(veh) = str_dup(argument);
    send_to_char(ch, "Position set. Your vehicle will appear as '(your vehicle) %s.^n'\r\n", GET_VEH_DEFPOS(veh));
  } else {
    DELETE_ARRAY_IF_EXTANT(GET_DEFPOS(ch));
    GET_DEFPOS(ch) = str_dup(argument);
    send_to_char(ch, "Position set. You will appear as '(your character) %s^n'\r\n", GET_DEFPOS(ch));
  }

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

    if (targ != ch) {
      mudlog_vfprintf(ch, LOG_WIZLOG, "%s viewing status/affects for %s.", GET_CHAR_NAME(ch), GET_CHAR_NAME(targ));
    }
  }

  char aff_buf[10000] = { '\0' };

  if (ch == targ)
    send_to_char("You are affected by:\r\n", ch);
  else send_to_char(ch, "%s is affected by:\r\n", GET_CHAR_NAME(targ));

  if (!IS_NPC(targ) && GET_POS(targ) == POS_MORTALLYW) {
    int min_body = -GET_BOD(targ) + (GET_BIOOVER(targ) > 0 ? GET_BIOOVER(targ) : 0);
    if ((int)((GET_PHYSICAL(targ) + 1) / 100) <= min_body) {
      snprintf(ENDOF(aff_buf), sizeof(aff_buf) - strlen(aff_buf), "  ^RBleeding Out ^r(^R%d^r ticks left until death)^n\r\n", GET_PC_SALVATION_TICKS(targ));
    } else {
      snprintf(ENDOF(aff_buf), sizeof(aff_buf) - strlen(aff_buf), "  ^rBleeding Out^n\r\n");
    }
  }

  if (targ->real_abils.esshole) {
    snprintf(ENDOF(aff_buf), sizeof(aff_buf) - strlen(aff_buf), "  Essence Hole (%.2f)\r\n", (float)targ->real_abils.esshole / 100);
  }
  if (GET_MAG(targ) && targ->real_abils.highestindex > GET_INDEX(targ)) {
    snprintf(ENDOF(aff_buf), sizeof(aff_buf) - strlen(aff_buf), "  Bioware Hole (%.2f)\r\n", (float)(targ->real_abils.highestindex - GET_INDEX(targ)) / 100);
  }
  switch (get_armor_penalty_grade(targ)) {
    case ARMOR_PENALTY_TOTAL:
      strlcat(aff_buf, "  Bulky Armor (Insane)\r\n", sizeof(aff_buf));
      break;
    case ARMOR_PENALTY_HEAVY:
      strlcat(aff_buf, "  Bulky Armor (Serious)\r\n", sizeof(aff_buf));
      break;
    case ARMOR_PENALTY_MEDIUM:
      strlcat(aff_buf, "  Bulky Armor (Moderate)\r\n", sizeof(aff_buf));
      break;
    case ARMOR_PENALTY_LIGHT:
      strlcat(aff_buf, "  Bulky Armor (Light)\r\n", sizeof(aff_buf));
      break;
  }
  if (GET_TEMP_QUI_LOSS(targ)) {
    snprintf(ENDOF(aff_buf), sizeof(aff_buf) - strlen(aff_buf), "  Temporary Quickness Loss: %d\r\n", GET_TEMP_QUI_LOSS(targ));
  }
  if (GET_TEMP_MAGIC_LOSS(targ)) {
    snprintf(ENDOF(aff_buf), sizeof(aff_buf) - strlen(aff_buf), "  Temporary Magic Loss: %d\r\n", GET_TEMP_MAGIC_LOSS(targ));
  }
  if (GET_TEMP_ESSLOSS(targ)) {
    snprintf(ENDOF(aff_buf), sizeof(aff_buf) - strlen(aff_buf), "  Temporary Essence Loss: %.2f\r\n", ((float) GET_TEMP_ESSLOSS(targ)) / 100);
  }
  if (GET_REACH(targ) && !(AFF_FLAGGED(targ, AFF_CLOSECOMBAT))) {
    snprintf(ENDOF(aff_buf), sizeof(aff_buf) - strlen(aff_buf), "  Extra Reach (%dm)\r\n", GET_REACH(targ));
  }
  if (AFF_FLAGGED(targ, AFF_CLOSECOMBAT)) {
    strlcat(aff_buf, "  Close Combat\r\n", sizeof(aff_buf));
  }
  if (AFF_FLAGGED(targ, AFF_RUTHENIUM)) {
    strlcat(aff_buf, "  Ruthenium\r\n", sizeof(aff_buf));
  }
  if (IS_PERCEIVING(targ) && !IS_DUAL(targ) && !IS_GHOUL(targ) && !IS_DRAGON(targ)) {
    strlcat(aff_buf, "  Astral Perception (^yincreased roll difficulties^n)\r\n", sizeof(aff_buf));
  }

  for (int bit_idx = 0; bit_idx < NUM_SPIRIT_POWER_BITS; bit_idx++) {
    int rating = affected_by_power(targ, bit_idx);
    if (rating) {
      snprintf(ENDOF(aff_buf), sizeof(aff_buf) - strlen(aff_buf), "  Spirit %s (%d)\r\n", spirit_powers_from_bit[bit_idx], rating);
    }
  }

  for (int i = MIN_DRUG; i < NUM_DRUGS; i++) {
    if (GET_DRUG_STAGE(targ, i) == DRUG_STAGE_ONSET || GET_DRUG_STAGE(targ, i) == DRUG_STAGE_COMEDOWN) {
      snprintf(ENDOF(aff_buf), sizeof(aff_buf) - strlen(aff_buf), "  %s (%s): %ds",
                   drug_types[i].name,
                   GET_DRUG_STAGE(targ, i) == DRUG_STAGE_ONSET ? "Onset" : "Comedown",
                   GET_DRUG_DURATION(targ, i)*2);
      switch (i) {
        case DRUG_ACTH:
          // Has no long-lasting effects. Nothing to display here.
          strlcat(aff_buf, "\r\n", sizeof(aff_buf));
          break;
        case DRUG_HYPER:
          if (GET_DRUG_STAGE(targ, i) == DRUG_STAGE_ONSET) {
            strlcat(aff_buf, " (+4 spellcasting roll difficulty, +1 other roll difficulties, take 50% bonus damage)\r\n", sizeof(aff_buf));
          } else {
            strlcat(aff_buf, "\r\n", sizeof(aff_buf));
          }
          break;
        case DRUG_JAZZ:
          if (GET_DRUG_STAGE(targ, i) == DRUG_STAGE_ONSET) {
            strlcat(aff_buf, " (+2 qui, +1d6 initiative)\r\n", sizeof(aff_buf));
          } else {
            strlcat(aff_buf, " (+1 spellcasting roll difficulty, -1 qui)\r\n", sizeof(aff_buf));
          }
          break;
        case DRUG_KAMIKAZE:
          if (GET_DRUG_STAGE(targ, i) == DRUG_STAGE_ONSET) {
            strlcat(aff_buf, " (+2 str, +1 bod/qui/wil, +1d6 initiative)\r\n", sizeof(aff_buf));
          } else {
            strlcat(aff_buf, " (-1 qui/wil)\r\n", sizeof(aff_buf));
          }
          break;
        case DRUG_PSYCHE:
          if (GET_DRUG_STAGE(targ, i) == DRUG_STAGE_ONSET) {
            strlcat(aff_buf, " (+1 int)\r\n", sizeof(aff_buf));
          } else {
            strlcat(aff_buf, "\r\n", sizeof(aff_buf));
          }
          break;
        case DRUG_BLISS:
          if (GET_DRUG_STAGE(targ, i) == DRUG_STAGE_ONSET) {
            strlcat(aff_buf, " (-1 rea, +1 roll difficulty, 3 levels of pain resistance)\r\n", sizeof(aff_buf));
          } else {
            strlcat(aff_buf, "\r\n", sizeof(aff_buf));
          }
          break;
        case DRUG_BURN:
          if (GET_DRUG_STAGE(targ, i) == DRUG_STAGE_ONSET) {
            strlcat(aff_buf, " (intoxicated)\r\n", sizeof(aff_buf));
          } else {
            strlcat(aff_buf, " (hung over)\r\n", sizeof(aff_buf));
          }
          break;
        case DRUG_CRAM:
          if (GET_DRUG_STAGE(targ, i) == DRUG_STAGE_ONSET) {
            strlcat(aff_buf, " (+1 rea, +1d6 initiative)\r\n", sizeof(aff_buf));
          } else {
            strlcat(aff_buf, "\r\n", sizeof(aff_buf));
          }
          break;
        case DRUG_NITRO:
          if (GET_DRUG_STAGE(targ, i) == DRUG_STAGE_ONSET) {
            strlcat(aff_buf, " (+2 str/wil, 6 levels of pain resistance, +2 to perception tests)\r\n", sizeof(aff_buf));
          } else {
            strlcat(aff_buf, "\r\n", sizeof(aff_buf));
          }
          break;
        case DRUG_NOVACOKE:
          if (GET_DRUG_STAGE(targ, i) == DRUG_STAGE_ONSET) {
            strlcat(aff_buf, " (+1 rea/cha, 1 level of pain resistance)\r\n", sizeof(aff_buf));
          } else {
            strlcat(aff_buf, " (crashed cha, halved wil)\r\n", sizeof(aff_buf));
          }
          break;
        case DRUG_ZEN:
          if (GET_DRUG_STAGE(targ, i) == DRUG_STAGE_ONSET) {
            strlcat(aff_buf, " (-2 rea, +1 roll difficulty, +1 wil)\r\n", sizeof(aff_buf));
          } else {
            strlcat(aff_buf, "\r\n", sizeof(aff_buf));
          }
          break;
        default:
          snprintf(buf, sizeof(buf), "SYSERR: Unknown drug type %d in aff!", i);
          mudlog(buf, targ, LOG_SYSLOG, TRUE);
          break;
      }
    }
    else if (GET_DRUG_ADDICT(targ, i) > 0) {
      if (GET_DRUG_STAGE(targ, i) == DRUG_STAGE_GUIDED_WITHDRAWAL) {
        snprintf(ENDOF(aff_buf), sizeof(aff_buf) - strlen(aff_buf), "  ^y%s Withdrawal (Guided, Edge %d): %s until next test^n\r\n",
                     drug_types[i].name,
                     GET_DRUG_ADDICTION_EDGE(targ, i),
                     get_time_until_withdrawal_test(targ, i));
      } else if (GET_DRUG_STAGE(targ, i) == DRUG_STAGE_FORCED_WITHDRAWAL) {
        snprintf(ENDOF(aff_buf), sizeof(aff_buf) - strlen(aff_buf), "  ^Y%s Withdrawal (Forced, Edge %d): %s until next test^n\r\n",
                     drug_types[i].name,
                     GET_DRUG_ADDICTION_EDGE(targ, i),
                     get_time_until_withdrawal_test(targ, i));
      } else {
        snprintf(ENDOF(aff_buf), sizeof(aff_buf) - strlen(aff_buf), "  Addicted to %s (Edge: %d)\r\n",
                     drug_types[i].name,
                     GET_DRUG_ADDICTION_EDGE(targ, i));
      }
    }
  }

  struct obj_data *patch = GET_EQ(targ, WEAR_PATCH);
  if (patch) {
    if (GET_OBJ_TYPE(patch) != ITEM_PATCH) {
      const char *representation = generate_new_loggable_representation(patch);
      mudlog_vfprintf(targ, LOG_SYSLOG, "SYSERR: Non-patch item %s being worn on patch location!", representation);
      delete [] representation;
    } else {
      snprintf(ENDOF(aff_buf), sizeof(aff_buf) - strlen(aff_buf), "  %s %s patch (rating ^c%d^n, ^c%d^n ticks left)\r\n",
              AN(patch_names[GET_PATCH_TYPE(patch)]),
              patch_names[GET_PATCH_TYPE(patch)],
              GET_PATCH_RATING(patch),
              GET_PATCH_TICKS_LEFT(patch));
    }
  }

  for (struct obj_data *bio = targ->bioware; bio; bio = bio->next_content) {
    if (GET_BIOWARE_TYPE(bio) == BIO_PAINEDITOR && GET_BIOWARE_IS_ACTIVATED(bio)) {
      strlcat(aff_buf, "  An activated pain editor (+1 wil, -1 int)\r\n", sizeof(aff_buf));
      break;
    }
  }

  for (struct sustain_data *sust = GET_SUSTAINED(targ); sust; sust = sust->next) {
    if (!sust->caster) {
      snprintf(buf, sizeof(buf), "  %s", spells[sust->spell].name);
      if (sust->spell == SPELL_INCATTR
          || sust->spell == SPELL_INCCYATTR
          || sust->spell == SPELL_DECATTR
          || sust->spell == SPELL_DECCYATTR)
      {
        snprintf(ENDOF(buf), sizeof(buf) - strlen(buf), "%s", attributes[sust->subtype]);
      } else if (SPELL_HAS_SUBTYPE(sust->spell)) {
        snprintf(ENDOF(buf), sizeof(buf) - strlen(buf), " (%s)", attributes[sust->subtype]);
      }
      if ((IS_SENATOR(ch) || sust->spell == SPELL_MINDLINK) && sust->other && sust->other != targ)
        snprintf(ENDOF(aff_buf), sizeof(aff_buf) - strlen(aff_buf), "%s (cast by ^c%s^n)\r\n", buf, GET_CHAR_NAME(sust->other));
      else
        snprintf(ENDOF(aff_buf), sizeof(aff_buf) - strlen(aff_buf), "%s\r\n", buf);
    }
  }

  if (GET_SUSTAINED_NUM(targ)) {
    snprintf(ENDOF(aff_buf), sizeof(aff_buf) - strlen(aff_buf), "\r\n%s %s sustaining:\r\n", ch == targ ? "You" : GET_CHAR_NAME(targ), ch == targ ? "are" : "is");
    int i = 1;
    for (struct sustain_data *sust = GET_SUSTAINED(targ); sust; sust = sust->next) {
      if (sust->caster || sust->spirit == targ) {
        snprintf(ENDOF(aff_buf), sizeof(aff_buf) - strlen(aff_buf), "%d) %s (force %d, %d successes%s)", i, get_spell_name(sust->spell, sust->subtype), sust->force, sust->success, warn_if_spell_under_potential(sust));
        if ((IS_SENATOR(ch) || sust->spell == SPELL_MINDLINK)) {
          snprintf(ENDOF(aff_buf), sizeof(aff_buf) - strlen(aff_buf), " (Cast on ^c%s^n)", GET_CHAR_NAME(sust->other));
        }
        if (sust->focus) {
          snprintf(ENDOF(aff_buf), sizeof(aff_buf) - strlen(aff_buf), " (Sustained by %s)", GET_OBJ_NAME(sust->focus));
        }
        if (sust->spirit && sust->spirit != ch) {
          snprintf(ENDOF(aff_buf), sizeof(aff_buf) - strlen(aff_buf), " (Sustained by %s [id %d])", GET_NAME(sust->spirit), GET_GRADE(sust->spirit));
        }
        strlcat(aff_buf, "\r\n", sizeof(aff_buf));
        i++;
      }
    }
  }

  if (!*aff_buf) {
    send_to_char("Nothing.\r\n", ch);
  } else {
    send_to_char(aff_buf, ch);
  }

  send_to_char(ch, "\r\n%s %s the following vision types:\r\n  %s\r\n",
               targ == ch ? "You" : GET_CHAR_NAME(targ),
               targ == ch ? "have" : "has",
               write_vision_string_for_display(targ, VISION_STRING_MODE_STATUS));

  if (GET_MAG(targ) > 0) {
    send_to_char("\r\n", ch);
    int force = 0;
    struct obj_data *focus;
    for (int x = 0; x < NUM_WEARS; x++) {
      if (!(focus = GET_EQ(targ, x)))
        continue;

      if (GET_OBJ_TYPE(focus) == ITEM_FOCUS && GET_FOCUS_BONDED_TO(focus) == GET_IDNUM(targ) && GET_FOCUS_ACTIVATED(focus)) {
        force += GET_FOCUS_FORCE(focus);
      }

      else if ((x == WEAR_WIELD || x == WEAR_HOLD) && GET_OBJ_TYPE(focus) == ITEM_WEAPON && WEAPON_IS_FOCUS(focus) && is_weapon_focus_usable_by(focus, targ)) {
        force += GET_WEAPON_FOCUS_RATING(focus);
      }
    }
    if (force == 0) {
      if (ch == targ)
        send_to_char("You're not using any foci.\r\n", ch);
      else
        send_to_char(ch, "%s is not using any foci.\r\n", GET_CHAR_NAME(targ));
    } else if (force > (GET_REAL_MAG(targ) / 100) * 2) {
      if (ch == targ)
        send_to_char(ch, "^YYou are at risk of geas from using too many points of foci! You're using %d, and need to reduce to %d.^n\r\n", force, (GET_REAL_MAG(targ) / 100) * 2);
      else
        send_to_char(ch, "^Y%s is at risk of geas from using too many points of foci! They're using %d, and need to reduce to %d.^n\r\n", GET_CHAR_NAME(targ), force, (GET_REAL_MAG(targ) / 100) * 2);
    } else {
      if (ch == targ)
        send_to_char(ch, "You're using a total of %d points of foci. If this gets above %d, you'll be at risk of geas.\r\n", force, (GET_REAL_MAG(targ) / 100) * 2);
      else
        send_to_char(ch, "%s is using a total of %d points of foci. If this gets above %d, they'll be at risk of geas.\r\n", GET_CHAR_NAME(targ), force, (GET_REAL_MAG(targ) / 100) * 2);
    }
  }

  if (GET_LEVEL(ch) > LVL_MORTAL) {
    render_drug_info_for_targ(ch, targ);
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

  if (!(obj = get_obj_in_list_vis(ch, buf1, ch->carrying))) {
    send_to_char(ch, "You don't have %s '%s' in your inventory.\r\n", AN(buf1), buf1);
    return;
  }

  if (!(vict = get_char_room_vis(ch, buf2))) {
    send_to_char(ch, "You don't see %s '%s' here.\r\n", AN(buf2), buf2);
    return;
  }

  act("You show $p to $N.", TRUE, ch, obj, vict, TO_CHAR);

  if (IS_IGNORING(vict, is_blocking_ic_interaction_from, ch)) {
    act("$n shows $p to $N.", TRUE, ch, obj, vict, TO_NOTVICT);
  } else {
    act("$n shows $p to $N.", TRUE, ch, obj, vict, TO_ROOM);
    show_obj_to_char(obj, vict, SHOW_MODE_JUST_DESCRIPTION);
  }
}

extern time_t get_idledelete_days_left(time_t lastd, int tke, int race, int rank);
ACMD(do_karma){
  send_to_char(ch, "You have ^c%d^n reputation and ^c%d^n notoriety. You've earned a lifetime total of ^c%d^n karma", GET_REP(ch), GET_NOT(ch), GET_TKE(ch));

  if (PLR_FLAGGED(ch, PLR_NODELETE)) {
    send_to_char(", and will never be idle-deleted.\r\n", ch);
  } else {
    time_t grace_days = get_idledelete_days_left(time(0), GET_TKE(ch), GET_RACE(ch), GET_LEVEL(ch));
    send_to_char(ch, ", giving you a total of ^c%ld^n days of idle-delete grace time (%s base of %d, plus 1 per %d TKE).\r\n",
                 grace_days,
                 IS_PRESTIGE_CH(ch) ? "prestige" : "standard",
                 IS_PRESTIGE_CH(ch) ? 365 : 50,
                 NUMBER_OF_TKE_POINTS_PER_REAL_DAY_OF_EXTRA_IDLE_DELETE_GRACE_PERIOD);
  }
}

#define STAFF_LEADERBOARD_SYNTAX_STRING "Syntax: leaderboard <option>, where option is one of: tke, reputation, notoriety, nuyen, syspoints, blocks, hardcore\r\n"
#define MORT_LEADERBOARD_SYNTAX_STRING "Syntax: leaderboard <option>, where option is one of: hardcore, gambling\r\n"
ACMD(do_leaderboard) {
  MYSQL_RES *res;
  MYSQL_ROW row;
  int counter = 1;

  // leaderboard <tke|rep|notor|nuyen|sysp|blocks>
  skip_spaces(&argument);
  if (!*argument) {
    send_to_char(GET_LEVEL(ch) > LVL_MORTAL ? STAFF_LEADERBOARD_SYNTAX_STRING : MORT_LEADERBOARD_SYNTAX_STRING, ch);
    return;
  }

  if (!strncmp(argument, "gambling", strlen(argument))) {
    display_gamba_ledger_leaderboard(ch);
    return;
  }

  if (!strncmp(argument, "hardcore", strlen(argument))) {
    mysql_wrapper(mysql, "SELECT name, tke FROM pfiles "
                         "  WHERE tke > 0 "
                         "  AND `rank` = 1 "
                         "  AND hardcore = 1 "
                         "  AND name != 'deleted' "
                         "ORDER BY tke DESC LIMIT 10;");

    if (!(res = mysql_use_result(mysql))) {
      mudlog_vfprintf(ch, LOG_SYSLOG, "SYSERR: Unable to look up hardcore leaderboard.");
      send_to_char(ch, "Sorry, the leaderboard system is offline at the moment.\r\n");
      return;
    }

    send_to_char(ch, "^CTop 10 hardcore characters by TKE:^n\r\n");

    while ((row = mysql_fetch_row(res))) {
      send_to_char(ch, "%2d) %-20s: %-15s\r\n", counter++, row[0], row[1]);
    }
    if (counter == 1)
      send_to_char(ch, "...Nobody! Looks like a great place to make your mark.\r\n");

    mysql_free_result(res);
    return;
  }

  const char *display_string = NULL, *query_string = NULL;

  if (GET_LEVEL(ch) > LVL_MORTAL) {
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

    else if (!strncmp(argument, "blocks", strlen(argument)) || !strncmp(argument, "ignores", strlen(argument))) {
      mysql_wrapper(mysql, "SELECT p.name, i.vict_idnum, COUNT(i.vict_idnum) AS `value_occurrence`"
                           "  FROM pfiles_ignore_v2 AS i"
                           "  JOIN pfiles AS p"
                           "    ON i.vict_idnum = p.idnum"
                           "  GROUP BY vict_idnum"
                           "  ORDER BY `value_occurrence` DESC LIMIT 10;");
      if (!(res = mysql_use_result(mysql))) {
      mudlog_vfprintf(ch, LOG_SYSLOG, "SYSERR: Unable to look up blocks/ignores leaderboard.");
        send_to_char(ch, "Sorry, the leaderboard system is offline at the moment.\r\n");
        return;
      }

      send_to_char(ch, "^CTop 10 most-blocked characters:^n\r\n");
      while ((row = mysql_fetch_row(res))) {
        int blocks = atoi(row[2]);

        send_to_char(ch, "%2d) %-20s: %d block%s.\r\n", counter++, row[0], blocks, blocks != 1 ? "s" : "");
      }

      mysql_free_result(res);
      return;
    }
  }

  if (!display_string || !query_string){
    send_to_char(GET_LEVEL(ch) > LVL_MORTAL ? STAFF_LEADERBOARD_SYNTAX_STRING : MORT_LEADERBOARD_SYNTAX_STRING, ch);
    return;
  }

  // Sanitization not required here-- they're constant strings.
  snprintf(buf, sizeof(buf), "SELECT name, %s FROM pfiles "
                             "  WHERE %s > 0 "
                             "  AND `rank` = 1 "
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

  FAILURE_CASE(!ch->in_room, "You must be standing in a room to do that.");

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

    if (has_secrets && success_test(GET_INT(ch), GET_JURISDICTION(ch->in_room) == JURISDICTION_SECRET ? 10 : 4))
      send_to_char("You feel like there's still something to uncover here...\r\n", ch);
  }
}

ACMD(do_wheresmycar) {
  /* wheresmycar <total amount to spend>

  Dispatches lunch-breaking wageslaves and street people to find your car.
  The distance they're willing to travel depends on how much you spend.
  */

  if (!ch->desc) {
    return;
  }

  if (GET_NUYEN_PAID_FOR_WHERES_MY_CAR(ch) != 0) {
    send_to_char("You've got a group out searching already. You'll have to wait until they return.\r\n", ch);
    return;
  }

  skip_spaces(&argument);
  int paid = atoi(argument);
  if (paid < WHERES_MY_CAR_MINIMUM_NUYEN_CHARGE) {
    send_to_char(ch, "Syntax: wheresmycar <nuyen amount to spend, minimum %d>\r\n", WHERES_MY_CAR_MINIMUM_NUYEN_CHARGE);
    return;
  }

  if (GET_NUYEN(ch) < paid) {
    send_to_char("You don't have that much paper nuyen on hand.\r\n", ch);
    return;
  }

  lose_nuyen(ch, paid, NUYEN_OUTFLOW_WHERESMYCAR);
  GET_NUYEN_PAID_FOR_WHERES_MY_CAR(ch) = paid;

  send_to_char(ch, "You gather up a few bored-looking passersby and pay out %d nuyen"
                   " to have them search for your vehicles, reserving the other half as a"
                   " reward for whoever finds it. They set off, and you prepare for a wait...\r\n", paid / 2);
}

/* Generates a few hints for the reader about what the object does and how you can interact with it.
   Displayed both when probing and when examining the object.                                         */
const char *get_command_hints_for_obj(struct obj_data *obj) {
  extern SPECIAL(clock);
  extern SPECIAL(hand_held_scanner);
  extern SPECIAL(anticoagulant);
  extern SPECIAL(toggled_invis);
  extern SPECIAL(desktop);
  extern SPECIAL(portable_gridguide);
  extern SPECIAL(pocket_sec);
  extern SPECIAL(trideo);
  extern SPECIAL(spraypaint);
  extern SPECIAL(floor_usable_radio);
  extern SPECIAL(medical_workshop);

  static char hint_string[1000];
  strlcpy(hint_string, "", sizeof(hint_string));

  if (GET_OBJ_SPEC(obj) == spraypaint) {
    strlcat(hint_string, "\r\nIt's used with the ^WSPRAY^n command.\r\n", sizeof(hint_string));
  }
  else if (GET_OBJ_SPEC(obj) == anticoagulant) {
    strlcat(hint_string, "\r\nIt's an anticoagulant, used to help control platelet factory bioware. Take it with the ^WEAT^n command.\r\n", sizeof(hint_string));
  }
  else if (GET_OBJ_SPEC(obj) == medical_workshop) {
    strlcat(hint_string, "\r\nIt enables the use of player cyberdoc commands (^WHELP CYBERDOC^n).\r\n", sizeof(hint_string));
  }
  else if (GET_OBJ_SPEC(obj) == floor_usable_radio) {
    strlcat(hint_string, "\r\nIt can be used even while on the ground.\r\n", sizeof(hint_string));
  }
  else if (GET_OBJ_SPEC(obj) == pocket_sec) {
    strlcat(hint_string, "\r\nIt's basically a smartphone that can't make calls. ^WUSE SECRETARY^n to start.\r\n", sizeof(hint_string));
  }
  else if (GET_OBJ_SPEC(obj) == portable_gridguide) {
    strlcat(hint_string, "\r\nIt lets you use the ^WGRID^n command while holding it.\r\n", sizeof(hint_string));
  }
  else if (GET_OBJ_SPEC(obj) == toggled_invis) {
    strlcat(hint_string, "\r\nYou can ^WACTIVATE^n and ^WDEACTIVATE^n the ruthenium fibers, granting invisibility.\r\n", sizeof(hint_string));
  }
  else if (GET_OBJ_SPEC(obj) == hand_held_scanner) {
    strlcat(hint_string, "\r\nWhile held, it will vibrate if you move and it detects someone nearby.\r\n", sizeof(hint_string));
  }
  else if (GET_OBJ_SPEC(obj) == clock) {
    strlcat(hint_string, "\r\nIt gives you a more accurate ^WTIME^n readout.\r\n", sizeof(hint_string));
  }
  else if (GET_OBJ_SPEC(obj) == desktop) {
    strlcat(hint_string, "\r\nIt's a computer for programming decking software on- drop it and ^WLIST^n to see its contents.\r\n", sizeof(hint_string));
  }
  else if (GET_OBJ_SPEC(obj) == trideo) {
    if (CAN_WEAR(obj, ITEM_WEAR_TAKE)) {
      strlcat(hint_string, "\r\nIt's a trideo unit, aka a Shadowrun TV-- you can ^WUSE^n it to turn it off and on.\r\n", sizeof(hint_string));
    } else {
      strlcat(hint_string, "\r\nIt's a trideo unit, aka a Shadowrun TV-- however, you can't use it.\r\n", sizeof(hint_string));
    }
  }

  switch (GET_OBJ_TYPE(obj)) {
    case ITEM_LIGHT:
      strlcat(hint_string, "\r\nYou can ^WWEAR^n and ^WREMOVE^n it.\r\n", sizeof(hint_string));
      break;
    case ITEM_WORN:
      strlcat(hint_string, "\r\nYou can ^WWEAR^n and ^WREMOVE^n it, and potentially ^WPUT^n things in it while worn. That exposes those stored things to combat damage, though!\r\n", sizeof(hint_string));
      break;
    case ITEM_WORKSHOP:
      switch (GET_WORKSHOP_GRADE(obj)) {
        case TYPE_KIT:
          if (GET_WORKSHOP_TYPE(obj) != TYPE_AMMO) {
            strlcat(hint_string, "\r\nHave it in your inventory to use it.\r\n", sizeof(hint_string));
          }
          break;
        case TYPE_WORKSHOP:
          strlcat(hint_string, "\r\nDrop it in your apartment or safe vehicle, then ^WUNPACK^n it to use it.\r\n", sizeof(hint_string));
          break;
        case TYPE_FACILITY:
          strlcat(hint_string, "\r\nDrop it in your apartment or safe vehicle it to use it.\r\n", sizeof(hint_string));
          break;
        default:
          mudlog("SYSERR: Came across unknown workshop grade in get_command_hints_for_obj()! Update the code.", NULL, LOG_SYSLOG, TRUE);
          break;
      }
      break;
    case ITEM_CAMERA:
      strlcat(hint_string, "\r\nYou can use the ^WPHOTO^n command while holding it.\r\n", sizeof(hint_string));
      break;
    case ITEM_PART:
      strlcat(hint_string, "\r\nIt's part of a deck. See ^WHELP CYBERDECK^n for details.\r\n", sizeof(hint_string)); //asdf write this file
      break;
    case ITEM_WEAPON:
      if (WEAPON_IS_GUN(obj)) {
        strlcat(hint_string, "\r\n^WWIELD^n it in combat for best results. You can also ^WHOLSTER^n it if you have the right equipment.\r\n", sizeof(hint_string));
      } else if (GET_WEAPON_ATTACK_TYPE(obj) == WEAP_GRENADE) {
        strlcat(hint_string, "\r\n^WWIELD^n it, then ^WTHROW^n it in a direction (ex: ^WTHROW NORTH^n).\r\n", sizeof(hint_string));
      } else {
        strlcat(hint_string, "\r\n^WWIELD^n it in combat for best results. You can also ^WSHEATHE^n it if you have the right equipment.\r\n", sizeof(hint_string));
      }
      break;
    case ITEM_FIREWEAPON:
    case ITEM_MISSILE:
      strlcat(hint_string, "\r\n^YBows / crossbows are not currently implemented.^n\r\n", sizeof(hint_string));
      break;
    case ITEM_CUSTOM_DECK:
      strlcat(hint_string, "\r\nIt's a custom cyberdeck, used with the matrix commands. See ^WHELP DECKING^n to begin.\r\n", sizeof(hint_string));
      break;
    case ITEM_GYRO:
      strlcat(hint_string, "\r\nIt absorbs recoil from heavy weapons when you ^WWEAR^n it.\r\n", sizeof(hint_string));
      break;
    case ITEM_DRUG:
      break;
    case ITEM_OTHER:
    case ITEM_KEY:
      // No specific commands for these, do nothing.
      break;
    case ITEM_MAGIC_TOOL:
      break;
    case ITEM_DOCWAGON:
      break;
    case ITEM_CONTAINER:
      break;
    case ITEM_RADIO:
      break;
    case ITEM_DRINKCON:
      break;
    case ITEM_FOOD:
      break;
    case ITEM_MONEY:
      break;
    case ITEM_PHONE:
      break;
    case ITEM_BIOWARE:
      break;
    case ITEM_FOUNTAIN:
      break;
    case ITEM_CYBERWARE:
      break;
    case ITEM_CYBERDECK:
      break;
    case ITEM_PROGRAM:
      break;
    case ITEM_GUN_MAGAZINE:
      break;
    case ITEM_GUN_ACCESSORY:
      break;
    case ITEM_SPELL_FORMULA:
      break;
    case ITEM_FOCUS:
      break;
    case ITEM_PATCH:
      break;
    case ITEM_CLIMBING:
      break;
    case ITEM_QUIVER:
      break;
    case ITEM_DECK_ACCESSORY:
      break;
    case ITEM_RCDECK:
      break;
    case ITEM_CHIP:
      break;
    case ITEM_MOD:
      break;
    case ITEM_HOLSTER:
      break;
    case ITEM_DESIGN:
      break;
    case ITEM_GUN_AMMO:
      strlcat(hint_string, "\r\nIt's used with the ^WPOCKETS^n command-- see ^WHELP POCKETS^n for details.\r\n", sizeof(hint_string));
      break;
    case ITEM_KEYRING:
      break;
    case ITEM_SHOPCONTAINER:
      if (obj->contains && (GET_OBJ_TYPE(obj->contains) == ITEM_CYBERWARE || GET_OBJ_TYPE(obj->contains) == ITEM_BIOWARE)) {
        strlcat(hint_string, "\r\nIt's used with cyberdoc commands, see ^WHELP CYBERDOC^n for details.\r\n", sizeof(hint_string));
      } else {
        mudlog("SYSERR: Received ITEM_SHOPCONTAINER to get_command_hints_for_obj that didn't contain cyberware or bioware! Update the code.", NULL, LOG_SYSLOG, TRUE);
      }
      break;
    case ITEM_VEHCONTAINER:
      break;
  }

  return hint_string;
}

void display_room_name(struct char_data *ch, struct room_data *in_room, bool in_veh) {
  if (!ch || !in_room) {
    mudlog("SYSERR: Received invalid character to display_room_name()!", ch, LOG_SYSLOG, TRUE);
    return;
  }

  const char *room_name = GET_ROOM_NAME(in_room);
  bool name_decorated = FALSE;

  if (GET_APARTMENT_SUBROOM(in_room) && GET_APARTMENT_SUBROOM(in_room)->get_decorated_name()) {
    room_name = GET_APARTMENT_SUBROOM(in_room)->get_decorated_name();
    name_decorated = TRUE;
  }

  room_name = replace_neutral_color_codes(room_name, "^C");

  {
    #define APPEND_ROOM_FLAG(check, flagname) { if ((check)) {strlcat(room_title_buf, flagname, sizeof(room_title_buf));} }
    char room_title_buf[2000];

    if ((PRF_FLAGGED(ch, PRF_ROOMFLAGS) && GET_REAL_LEVEL(ch) >= LVL_BUILDER)) {
      // Write the room title with bits after it.
      ROOM_FLAGS(in_room).PrintBits(buf, MAX_STRING_LENGTH, room_bits, ROOM_MAX);
      snprintf(room_title_buf, sizeof(room_title_buf), "^C[%5ld] %s^n%s [ %s ] {%s}^n",
                  GET_ROOM_VNUM(in_room), 
                  room_name,
                  name_decorated ? " ^L(name-dec'd)^n" : "",
                  buf,
                  spirit_name_with_hearth[SECT(in_room)]);
      // Append things that don't show up in bits.
      APPEND_ROOM_FLAG(IS_WATER(in_room), " ^B(Flooded)^n");
      APPEND_ROOM_FLAG((in_room->matrix && real_host(in_room->matrix) >= 1), " (Jackpoint)");
      if (in_room->flight_code && (ROOM_FLAGGED(in_room, ROOM_HELIPAD) || ROOM_FLAGGED(in_room, ROOM_RUNWAY))) {
        snprintf(ENDOF(room_title_buf), sizeof(room_title_buf) - strlen(room_title_buf), " (%s: %3s)", 
                ROOM_FLAGGED(in_room, ROOM_RUNWAY) ? "Runway" : "Helipad",
                in_room->flight_code);
      }
      // Add apartment info.
      if (GET_APARTMENT(in_room)) {
        snprintf(ENDOF(room_title_buf), sizeof(room_title_buf) - strlen(room_title_buf), "\r\n ^c(%sApartment - %s^c%s)",
                    GET_APARTMENT(in_room)->get_paid_until() > 0 ? "Leased " : "",
                    GET_APARTMENT(in_room)->get_full_name(),
                    GET_APARTMENT_DECORATION(in_room) ? " [decorated]" : "");
      }
      // Add tempdesc info.
      if (in_room->temp_desc && in_room->temp_desc_timeout > 0) {
        const char *author_name = get_player_name(in_room->temp_desc_author_idnum);
        snprintf(ENDOF(room_title_buf), sizeof(room_title_buf) - strlen(room_title_buf), "\r\n ^c(Temp Desc by %s (%ld) for next %d minute%s)", 
                    author_name,
                    in_room->temp_desc_author_idnum,
                    in_room->temp_desc_timeout,
                    in_room->temp_desc_timeout == 1 ? "" : "s");
        delete [] author_name;
      }
    } else {
      snprintf(room_title_buf, sizeof(room_title_buf), "^C%s%s^n", in_veh ? "Around you is " : "", room_name);

      APPEND_ROOM_FLAG(ROOM_FLAGGED(in_room, ROOM_GARAGE), " (Garage)");
      APPEND_ROOM_FLAG(ROOM_FLAGGED(in_room, ROOM_STORAGE) && !ROOM_FLAGGED(in_room, ROOM_CORPSE_SAVE_HACK), " (Storage)");
      if (GET_APARTMENT(in_room)) {
        snprintf(ENDOF(room_title_buf), sizeof(room_title_buf) - strlen(room_title_buf), " (%s-Class Apartment)",
                lifestyles[GET_APARTMENT(in_room)->get_lifestyle()].name);
      }
      APPEND_ROOM_FLAG(ROOM_FLAGGED(in_room, ROOM_STERILE), " (Sterile)");
      APPEND_ROOM_FLAG(ROOM_FLAGGED(in_room, ROOM_ARENA), " ^y(Arena)^n");
      APPEND_ROOM_FLAG(ROOM_FLAGGED(in_room, ROOM_PEACEFUL), " (Peaceful)");
      APPEND_ROOM_FLAG(IS_WATER(in_room), " ^B(Flooded)^n");
      APPEND_ROOM_FLAG((in_room->matrix && real_host(in_room->matrix) >= 1), " (Jackpoint)");
      APPEND_ROOM_FLAG(ROOM_FLAGGED(in_room, ROOM_ENCOURAGE_CONGREGATION), " ^W(Socialization Bonus)^n");

      if (in_room->flight_code && (ROOM_FLAGGED(in_room, ROOM_HELIPAD) || ROOM_FLAGGED(in_room, ROOM_RUNWAY))) {
        snprintf(ENDOF(room_title_buf), sizeof(room_title_buf), " (%s: %3s)", 
                ROOM_FLAGGED(in_room, ROOM_RUNWAY) ? "Runway" : "Helipad",
                in_room->flight_code);
      }

      if (in_room->temp_desc && in_room->temp_desc_timeout > 0) {
        strlcat(room_title_buf, " ^c(Altered)^n", sizeof(room_title_buf));
      }
    }

    send_to_char(ch, "%s\r\n", room_title_buf);
    #undef APPEND_ROOM_FLAG
  }
}

void display_room_desc(struct char_data *ch) {
  if (!ch || !ch->in_room) {
    mudlog("SYSERR: Received invalid character to display_room_desc()!", ch, LOG_SYSLOG, TRUE);
    return;
  }

  send_to_char(get_room_desc(ch->in_room), ch);
}

ACMD(do_penalties) {
  int total_damage_modifier = 0, total_sustain_modifier = 0, total_modify_target_mundane_action = 0, total_modify_target_magical_action = 0, skill_idx = -1;
  char msg_buffer[20000] = {0};
  char dummy_rbuf[10] = {0};
  struct char_data *vict = ch;
  char skill_name[MAX_INPUT_LENGTH] = {0};


  skip_spaces(&argument);

  // Staff syntax: PENALTIES [target] [skillname]
  if (GET_LEVEL(ch) >= LVL_BUILDER && *argument) {
    char targ_name[MAX_INPUT_LENGTH];
    const char *remainder = one_argument(argument, targ_name);
    strlcpy(skill_name, remainder, sizeof(skill_name));

    vict = get_char_room_vis(ch, targ_name);
    if (!vict)
      vict = get_char_vis(ch, targ_name);
    if (!vict) {
      vict = ch;
      send_to_char(ch, "Didn't find a character named '%s', so I'll treat your whole argument as a skill name!\r\n", targ_name);
      strlcpy(skill_name, argument, sizeof(skill_name));
    }
  } 
  // Player syntax: PENALTIES [skillname]
  else {
    strlcpy(skill_name, argument, sizeof(skill_name));
  }

  if (*skill_name) {
    if ((skill_idx = find_skill_num(skill_name)) < 0) {
      send_to_char(ch, "Sorry, '%s' is not a valid skill name.\r\n", skill_name);
      return;
    }

    if (skills[skill_idx].no_defaulting_allowed && GET_SKILL(vict, skill_idx) <= 0) {
      send_to_char(ch, "%s can't use %s without at least one rank in it.\r\n", 
                   vict == ch ? "You" : GET_CHAR_NAME(vict), 
                   skills[skill_idx].name);
      return;
    }

    int dummy_target = 4;
    int total_skill = get_skill(vict, skill_idx, dummy_target, msg_buffer, sizeof(msg_buffer));
    int total_penalty = dummy_target - 4;

    if (total_skill <= 0) {
      send_to_char(ch, "%s will roll ^c%d^n dice with %s, so will always fail.\r\n", 
                   vict == ch ? "You" : GET_CHAR_NAME(vict),
                   total_skill,
                   skills[skill_idx].name);
      return;
    }

    total_penalty += damage_modifier(vict, dummy_rbuf, sizeof(dummy_rbuf), msg_buffer, sizeof(msg_buffer));
    total_penalty += sustain_modifier(vict, dummy_rbuf, sizeof(dummy_rbuf), FALSE, msg_buffer, sizeof(msg_buffer));
    if (skills[skill_idx].requires_magic) {
      total_penalty += modify_target_rbuf_raw(vict, dummy_rbuf, sizeof(dummy_rbuf), 0, TRUE, msg_buffer, sizeof(msg_buffer));
    } else {
      total_penalty += modify_target_rbuf_raw(vict, dummy_rbuf, sizeof(dummy_rbuf), 0, FALSE, msg_buffer, sizeof(msg_buffer));
    }

    if (total_penalty == 0) {
      send_to_char(ch, "%s will roll ^c%d^n dice and will not have any TN modifiers when using %s.\r\n", 
                   vict == ch ? "You" : GET_CHAR_NAME(vict),
                   total_skill,
                   skills[skill_idx].name);
      return;
    }

    send_to_char(ch, "%s will roll ^c%d^n dice and take ^c%s%d^n in TN penalties when using %s:\r\n%s", 
                 vict == ch ? "You" : GET_CHAR_NAME(vict),
                 total_skill,
                 total_penalty > 0 ? "+" : "",
                 total_penalty,
                 skills[skill_idx].name,
                 msg_buffer);
    return;
  }

  send_to_char(ch, "%s %s affected by the following penalties:\r\n", 
               vict == ch ? "You" : GET_CHAR_NAME(vict),
               vict == ch ? "are" : "is");
  
  total_damage_modifier = damage_modifier(vict, dummy_rbuf, sizeof(dummy_rbuf), msg_buffer, sizeof(msg_buffer));
  if (*msg_buffer && total_damage_modifier) {
    send_to_char(ch, "Damage penalties (subtotal ^c%d^n):\r\n%s", total_damage_modifier, msg_buffer);
  } else {
    send_to_char("No damage penalties.\r\n", ch);
  }
  *msg_buffer = '\0';

  if (GET_MAG(vict) > 0) {
    total_sustain_modifier = sustain_modifier(vict, dummy_rbuf, sizeof(dummy_rbuf), FALSE, msg_buffer, sizeof(msg_buffer));
    if (*msg_buffer && total_sustain_modifier) {
      send_to_char(ch, "\r\nSpell sustaining penalties (subtotal ^c%d^n):\r\n%s", total_sustain_modifier, msg_buffer);
    } else {
      send_to_char("\r\nNo spell sustaining penalties.\r\n", ch);
    }
    *msg_buffer = '\0';

    send_to_char(ch, "\r\nThe following general penalties apply when performing ^cmagical^n actions:\r\n");
    total_modify_target_magical_action = modify_target_rbuf_raw(vict, dummy_rbuf, sizeof(dummy_rbuf), 0, TRUE, msg_buffer, sizeof(msg_buffer));
    if (*msg_buffer && total_modify_target_magical_action) {
      send_to_char(ch, " General penalties (subtotal ^c%d^n):\r\n%s", total_modify_target_magical_action, msg_buffer);
    } else {
      send_to_char(" No general penalties.\r\n", ch);
    }
    *msg_buffer = '\0';

    send_to_char(ch, "\r\nThe following general penalties apply when performing ^cmundane^n actions:\r\n");
    total_modify_target_mundane_action = modify_target_rbuf_raw(vict, dummy_rbuf, sizeof(dummy_rbuf), 0, FALSE, msg_buffer, sizeof(msg_buffer));
    if (*msg_buffer && total_modify_target_mundane_action) {
      send_to_char(ch, " General penalties (subtotal ^c%d^n):\r\n%s", total_modify_target_mundane_action, msg_buffer);
    } else {
      send_to_char(" No general penalties.\r\n", ch);
    }
    *msg_buffer = '\0';

    send_to_char(ch, "\r\nIn total, you'll take ^C%d^n in TN penalties when performing magic, ^C%d^n otherwise.\r\n", 
                 total_damage_modifier + total_sustain_modifier + total_modify_target_magical_action,
                 total_damage_modifier + total_sustain_modifier + total_modify_target_mundane_action);
  } else {
    total_modify_target_mundane_action = modify_target_rbuf_raw(vict, dummy_rbuf, sizeof(dummy_rbuf), 0, FALSE, msg_buffer, sizeof(msg_buffer));
    if (*msg_buffer && total_modify_target_mundane_action) {
      send_to_char(ch, "\r\nGeneral penalties (subtotal ^c%d^n):\r\n%s", total_modify_target_mundane_action, msg_buffer);
    } else {
      send_to_char("\r\nNo general penalties.\r\n", ch);
    }
    *msg_buffer = '\0';

    send_to_char(ch, "\r\nIn total, you'll take ^C%d^n in TN penalties.\r\n",
                 total_damage_modifier + total_sustain_modifier + total_modify_target_mundane_action);
  }
}

int crapcount_target(struct char_data *victim, struct char_data *viewer) {
  long total_value_of_money_items = 0;

  // On their person.
  int total_crap = count_objects_on_char(victim, total_value_of_money_items);
  if (viewer) {
    if (IS_SENATOR(viewer)) {
      send_to_char(viewer, "%15s - Carrying / equipped / cyberware / bioware (%ld nuyen contents)\r\n",
                  get_crap_count_string(total_crap, "^n", PRF_FLAGGED(viewer, PRF_SCREENREADER)),
                  total_value_of_money_items);
    } else {
      send_to_char(viewer, "%15s - Carrying / equipped / cyberware / bioware\r\n", get_crap_count_string(total_crap, "^n", PRF_FLAGGED(viewer, PRF_SCREENREADER)));
    }
  }

  // Apartments.
  for (auto *complex : global_apartment_complexes) {
    long cash_value = 0;

    for (auto *apartment : complex->get_apartments()) {
      if (apartment->owner_is_valid() && apartment->get_owner_id() == GET_IDNUM_EVEN_IF_PROJECTING(victim)) {
        int crap_count = 0;
        for (auto *room : apartment->get_rooms()) {
          crap_count += count_objects_in_room(room->get_world_room(), cash_value);
        }

        if (viewer) {
          if (IS_SENATOR(viewer)) {
            send_to_char(viewer, "%15s - %s (vnum %ld, nuyen contents %ld)\r\n",
                        get_crap_count_string(crap_count, "^n", PRF_FLAGGED(viewer, PRF_SCREENREADER)),
                        apartment->get_full_name(),
                        apartment->get_root_vnum(),
                        cash_value);
          } else {
            send_to_char(viewer, "%15s - %s\r\n",
                        get_crap_count_string(crap_count, "^n", PRF_FLAGGED(viewer, PRF_SCREENREADER)),
                        apartment->get_full_name());
          }
        }
        
        total_crap += crap_count;
        total_value_of_money_items += cash_value;
      }
    }
  }

  // Vehicles.
  for (struct veh_data *veh = veh_list; veh; veh = veh->next) {
    long cash_value = 0;

    if (veh->owner == GET_IDNUM_EVEN_IF_PROJECTING(victim)) {
      int crap_count = count_objects_in_veh(veh, cash_value);

      if (viewer) {
        if (IS_SENATOR(viewer)) {
          send_to_char(viewer, "%15s - %s (at %ld, nuyen contents %ld)\r\n",
                      get_crap_count_string(crap_count, "^n", PRF_FLAGGED(viewer, PRF_SCREENREADER)),
                      GET_VEH_NAME(veh),
                      get_veh_in_room(veh) ? GET_ROOM_VNUM(get_veh_in_room(veh)) : -1,
                      cash_value);
        } else {
          send_to_char(viewer, "%15s - %s\r\n",
                      get_crap_count_string(crap_count, "^n", PRF_FLAGGED(viewer, PRF_SCREENREADER)),
                      GET_VEH_NAME(veh));
        }
      }
      
      total_crap += crap_count;
      total_value_of_money_items += cash_value;
    }
  }

  // Total.
  if (viewer) {
    if (IS_SENATOR(viewer)) {
      send_to_char(viewer, "Total: %s (%ld nuyen stored in credsticks etc).\r\n", 
                  get_crap_count_string(total_crap, "^n", PRF_FLAGGED(viewer, PRF_SCREENREADER)),
                  total_value_of_money_items);
    } else {
      send_to_char(viewer, "Total: %s.\r\n", get_crap_count_string(total_crap, "^n", PRF_FLAGGED(viewer, PRF_SCREENREADER)));
    }
  }

  return total_crap;
}

// Iterate through all active connections and warn them if their crapcount is high.
void send_crapcount_warnings() {
  for (struct descriptor_data *d = descriptor_list; d; d = d->next) {
    struct char_data *vict = d->original ? d->original : d->character;
    if (d->idle_ticks < 60 && vict) {
      int crap_count = crapcount_target(vict, NULL);

      if (crap_count >= CRAP_COUNT_EXTREME) {
        send_to_char(vict, "(OOC note): ^RYour total item count is excessively high at %d.^n\r\n"
                           "Please sell or junk unwanted items. If you are unable to do so, staff will assist in reducing your item count.\r\n", crap_count);
      } else if (crap_count >= CRAP_COUNT_VERY_HIGH) {
        send_to_char(vict, "(OOC note): ^rYour total item count is very high at ^R%d^r.^n\r\n"
                           "Please sell or junk unwanted items. If you'd like, staff can assist in reducing your item count.\r\n", crap_count);
      } else if (crap_count >= CRAP_COUNT_HIGH) {
        send_to_char(vict, "(OOC note): Your total item count is high at %d. Please consider selling or junking unwanted items to reduce game load.\r\n", crap_count);
      }
    }
  }
}

ACMD(do_count) {
  skip_spaces(&argument);
  long dummy_cash = 0;

  if (!*argument) {
    send_to_char("Syntax: ^WCOUNT INV^n  (to count nested things you're carrying)\r\n"
                 "        ^WCOUNT ALL^n  (to count all your stuff across the game.)\r\n", ch);
    
    if (IS_SENATOR(ch)) {
      send_to_char("        ^WCOUNT <online target>^n (count that target's things)\r\n", ch);
      send_to_char("        ^WCOUNT *^n               (get aggregate counts for everyone online [SLOW])\r\n", ch);
    }

    return;
  }

  // Count all crap across the game that belongs to you.
  if (!str_cmp(argument, "all")) {
    send_to_char("Counting all of your character's stuff across the game...\r\n", ch);
    crapcount_target(ch, ch);
    send_to_char("\r\nIf this seems high, note that each individual pocket secretary mail is an item. Deleting old mail etc will help.\r\n", ch);
    return;
  }

  // Staff can run a count on a single person as well.
  if (IS_SENATOR(ch)) {
    if (*argument == '*') {
      FAILURE_CASE(!access_level(ch, LVL_PRESIDENT), "Sorry, for load reasons this is only available to game owners.");

      int total_count = 0;

      for (struct descriptor_data *d = descriptor_list; d; d = d->next) {
        struct char_data *vict = d->original ? d->original : d->character;
        if (vict) {
          int crapcount_amt = crapcount_target(vict, NULL);
          total_count += crapcount_amt;
          send_to_char(ch, "%-24s: %s\r\n", GET_CHAR_NAME(vict), get_crap_count_string(crapcount_amt));
        }
      }
      
      send_to_char(ch, "Total count across all online PCs: %s\r\n", get_crap_count_string(total_count));
      return;
    }

    struct char_data *vict = get_player_vis(ch, argument, FALSE);
    if (vict) {
      send_to_char(ch, "Counting all of %s's stuff across the game...\r\n", GET_CHAR_NAME(vict));
      crapcount_target(vict, ch);
      return;
    }
  }

  // Count all the nested stuff you're carrying.
  for (struct obj_data *carried = ch->carrying; carried; carried = carried->next_content) {
    int crap_count = count_object_including_contents(carried, dummy_cash) - 1;

    if (crap_count) {
      send_to_char(ch, "^cCarried:^n %s (contains %s)\r\n", decapitalize_a_an(GET_OBJ_NAME(carried)), get_crap_count_string(crap_count, "^n", PRF_FLAGGED(ch, PRF_SCREENREADER)));
    }
  }

  // Same for worn items.
  for (int wear_idx = 0; wear_idx < NUM_WEARS; wear_idx++) {
    if (!GET_EQ(ch, wear_idx))
      continue;
      
    int crap_count = count_object_including_contents(GET_EQ(ch, wear_idx), dummy_cash) - 1;

    if (crap_count) {
      send_to_char(ch, "^cWorn:^n    %s (contains %s)\r\n", decapitalize_a_an(GET_OBJ_NAME(GET_EQ(ch, wear_idx))), get_crap_count_string(crap_count, "^n", PRF_FLAGGED(ch, PRF_SCREENREADER)));
    }
  }

  send_to_char("\r\nIf any of this seems high, note that each individual pocket secretary mail is an item. Deleting old mail etc will help.\r\n", ch);
}

void write_gsgp_file(int player_count, const char *path) {
  FILE *fl;
  if (!(fl = fopen(path, "w"))) {
    mudlog_vfprintf(NULL, LOG_SYSLOG, "SYSERR: Cannot open file %s for write", path);
    return;
  }

  // Write our GSGP file per https://game-scry.online/about?goto=gsgp
  fprintf(fl, "{\r\n");
  fprintf(fl, "    \"name\": \"AwakeMUD CE\",\r\n");
  fprintf(fl, "    \"active_players\": %d\r\n", player_count);
  fprintf(fl, "}");

  fclose(fl);
}