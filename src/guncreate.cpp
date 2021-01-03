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
#include "bullet_pants.h"

#define CH d->character
#define OBJ d->edit_obj

#define AEDIT_MENU	0
#define AEDIT_WEAPON	1
#define AEDIT_TYPE	2
#define AEDIT_QUANTITY	3

void aedit_disp_menu(struct descriptor_data *d)
{
  CLS(CH);
  send_to_char(CH, "1) Weapon: ^c%s^n\r\n", weapon_type[GET_AMMOBOX_WEAPON(OBJ)]);
  send_to_char(CH, "2) Type: ^c%s^n\r\n", ammo_type[GET_AMMOBOX_TYPE(OBJ)].name);
  send_to_char(CH, "3) Amount: ^c%d^n\r\n", GET_AMMOBOX_INTENDED_QUANTITY(OBJ));
  send_to_char(CH, "q) Quit\r\nEnter your choice: ");
  d->edit_mode = AEDIT_MENU;
}

void create_ammo(struct char_data *ch)
{
  struct obj_data *ammo = read_object(OBJ_BLANK_AMMOBOX, VIRTUAL);
  STATE(ch->desc) = CON_AMMO_CREATE;
  GET_AMMOBOX_WEAPON(ammo) = WEAP_HOLDOUT;
  GET_AMMOBOX_INTENDED_QUANTITY(ammo) = 10;
  ch->desc->edit_obj = ammo;
  aedit_disp_menu(ch->desc);
}

void aedit_disp_weapon_menu(struct descriptor_data *d)
{
  CLS(CH);
  for (int counter = 0; counter < WEAP_CANNON - WEAP_HOLDOUT; counter += 2)
  {
    send_to_char(CH, "  %2d) %-15s %2d) %s\r\n",
                 counter, weapon_type[counter + WEAP_HOLDOUT],
                 counter + 1, counter + 1 < WEAP_CANNON - WEAP_HOLDOUT ?
                 weapon_type[counter + 1 + WEAP_HOLDOUT] : "(none)");
  }
  send_to_char("Weapon type: ", d->character);
  d->edit_mode = AEDIT_WEAPON;
}

void aedit_disp_type_menu(struct descriptor_data *d)
{
  CLS(CH);
  for (int counter = 0; counter <= AMMO_GEL; counter++)
    send_to_char(CH, "  %2d) %s\r\n", counter, ammo_type[counter].name);
  send_to_char("Ammo type: ", d->character);
  d->edit_mode = AEDIT_TYPE;  
}

void aedit_parse(struct descriptor_data *d, const char *arg)
{
  int number = atoi(arg);
  switch(d->edit_mode)
  {
  case AEDIT_MENU:
    switch (*arg) {
    case '1':
      aedit_disp_weapon_menu(d);
      break;
    case '2':
      aedit_disp_type_menu(d);
      break;
    case '3':
      send_to_char(CH,"Enter Quantity (In multiples of 10): ");
      d->edit_mode = AEDIT_QUANTITY;
      break;
    case 'q':
      send_to_char(CH, "Ammo design saved!\r\n");
      snprintf(buf, sizeof(buf), "a box of %s %s ammunition", ammo_type[GET_AMMOBOX_TYPE(OBJ)].name, weapon_type[GET_AMMOBOX_WEAPON(OBJ)]);
      OBJ->restring = str_dup(buf);
      GET_AMMOBOX_CREATOR(OBJ) = GET_IDNUM(CH);
      obj_to_char(OBJ, CH);
      STATE(d) = CON_PLAYING;
      OBJ = NULL;
      break;
    }
    break;
  case AEDIT_TYPE:
   if (number > AMMO_GEL || number < AMMO_NORMAL)
     send_to_char("Invalid selection.\r\nAmmo Type: ", CH);
   else {
     GET_AMMOBOX_TYPE(OBJ) = number;
     aedit_disp_menu(d);
   }
   break; 
  case AEDIT_WEAPON:
   number += WEAP_HOLDOUT;
   if (number >= WEAP_CANNON || number < WEAP_HOLDOUT)
     send_to_char("Invalid selection.\r\nWeapon Type: ", CH);
   else {
     GET_AMMOBOX_WEAPON(OBJ) = number;
     aedit_disp_menu(d);
   }
   break; 
  case AEDIT_QUANTITY:
   if (number < 0)
     number = 0;
   GET_AMMOBOX_INTENDED_QUANTITY(OBJ) = (number / 10) * 10;
   aedit_disp_menu(d);
   break;
  }
}

int gunsmith_skill(int weapon_type)
{
  switch (weapon_type) {
    case WEAP_HOLDOUT:
    case WEAP_LIGHT_PISTOL:
    case WEAP_MACHINE_PISTOL:
    case WEAP_HEAVY_PISTOL:
      return SKILL_BR_PISTOL;
    case WEAP_SMG:
      return SKILL_BR_SMG;
    case WEAP_SPORT_RIFLE:
    case WEAP_SNIPER_RIFLE:
    case WEAP_ASSAULT_RIFLE:
      return SKILL_BR_RIFLE;
    case WEAP_SHOTGUN:
      return SKILL_BR_SHOTGUN;
    case WEAP_LMG:
    case WEAP_MMG:
    case WEAP_HMG:
    case WEAP_MINIGUN:
      return SKILL_BR_HEAVYWEAPON;
  }
  return -1;
}

bool ammo_test(struct char_data *ch, struct obj_data *obj)
{
  if (GET_NUYEN(ch) <= get_ammo_cost(GET_AMMOBOX_WEAPON(obj), GET_AMMOBOX_TYPE(obj)) * 10) {
    send_to_char(ch, "You don't have enough nuyen for the materials to create %s.\r\n",
                 GET_OBJ_NAME(obj));
    if (IS_WORKING(ch))
      STOP_WORKING(ch);
    return FALSE;
  }
  int skillnum = gunsmith_skill(GET_AMMOBOX_WEAPON(obj)), target = ammo_type[GET_AMMOBOX_TYPE(obj)].tn;
  int skill = get_skill(ch, skillnum, target);
  int success = success_test(skill, target);
  int csuccess = MIN((int)(success * ((float)GET_COST_BREAKUP(ch) / 100)), 10);
  if (csuccess == success)
    csuccess--;
  snprintf(buf, sizeof(buf), "AmmoTest: Skill %d, Target %d, Success %d(c%d/t%d)", skill, target, success, csuccess, success - csuccess);
  act(buf, FALSE, ch, NULL, NULL, TO_ROLLS);
  if (success > 0)
    GET_AMMOBOX_TIME_TO_COMPLETION(obj) = (int)((ammo_type[GET_AMMOBOX_TYPE(obj)].time / MAX(success - csuccess, 1)) * 60);
  else
    GET_AMMOBOX_TIME_TO_COMPLETION(obj) = -1;
  GET_NUYEN(ch) -= (int)((get_ammo_cost(GET_AMMOBOX_WEAPON(obj), GET_AMMOBOX_TYPE(obj)) * 10) * (1 - (csuccess * .05)));
  GET_OBJ_VAL(obj, 10) = GET_AMMOBOX_TIME_TO_COMPLETION(obj);
  return TRUE;
}

void ammo_build(struct char_data *ch, struct obj_data *obj)
{
  if (GET_AMMOBOX_CREATOR(obj) != GET_IDNUM(ch))
    send_to_char("You cannot build this batch of ammunition.\r\n", ch);
  else {
    // Find a deployed workshop in the room.
    struct obj_data *workshop = find_workshop(ch, TYPE_AMMO);
    bool kitwarn = FALSE;

    if (!workshop) {
      // Find a kit in the character's inventory.
      for (workshop = ch->carrying; workshop; workshop = workshop->next_content) {
        if (GET_OBJ_TYPE(workshop) == ITEM_WORKSHOP && GET_WORKSHOP_TYPE(workshop) == TYPE_AMMO &&
            GET_WORKSHOP_GRADE(workshop) == TYPE_KIT) {
          // Ensure that, if a kit was found, the kit is of the right type.
          if (GET_AMMOBOX_TYPE(obj) == AMMO_NORMAL || (GET_AMMOBOX_TYPE(obj) == GET_WORKSHOP_AMMOKIT_TYPE(workshop)))
            break;
          else
            kitwarn = TRUE;
        }
        
      }
    }

    if (!workshop) {
      if (kitwarn) {
        send_to_char(ch, "Your ammunition kit doesn't have the right tooling for %s ammo. You'll need a different kit or an ammunition workshop.\r\n",
                     ammo_type[GET_AMMOBOX_TYPE(obj)].name);
      } else {
        if (GET_AMMOBOX_TYPE(obj) != AMMO_NORMAL) {
          send_to_char(ch, "You need an ammunition workshop or %s %s ammunition kit.\r\n",
                       strchr((const char *)"aeiouyAEIOUY", *ammo_type[GET_AMMOBOX_TYPE(obj)].name) ? "an" : "a",
                       ammo_type[GET_AMMOBOX_TYPE(obj)].name);
        } else {
          send_to_char("You need an ammunition workshop or kit.\r\n", ch);
        }
      }
      return;
    }

    if (GET_AMMOBOX_TIME_TO_COMPLETION(obj))
      send_to_char(ch, "You continue working on %s.\r\n", GET_OBJ_NAME(obj));
    else {
      send_to_char(ch, "You start working on %s.\r\n", GET_OBJ_NAME(obj));
      if (!ammo_test(ch, obj))
        return;
    }
    AFF_FLAGS(ch).SetBit(AFF_AMMOBUILD);
    GET_BUILDING(ch) = obj;  
  }
}
