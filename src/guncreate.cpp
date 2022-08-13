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
#include "bullet_pants.hpp"
#include "config.hpp"

#define CH d->character
#define OBJ d->edit_obj

#define AEDIT_MENU	0
#define AEDIT_WEAPON	1
#define AEDIT_TYPE	2
#define AEDIT_QUANTITY	3

void aedit_disp_menu(struct descriptor_data *d)
{
  CLS(CH);
  send_to_char(CH, "1) Weapon: ^c%s^n\r\n", weapon_types[GET_AMMOBOX_WEAPON(OBJ)]);
  send_to_char(CH, "2) Type: ^c%s^n\r\n", ammo_type[GET_AMMOBOX_TYPE(OBJ)].name);
  send_to_char(CH, "3) Amount: ^c%d^n  (^c%d^n nuyen)\r\n", GET_AMMOBOX_INTENDED_QUANTITY(OBJ), get_ammo_cost(GET_AMMOBOX_WEAPON(OBJ), GET_AMMOBOX_TYPE(OBJ)) * GET_AMMOBOX_INTENDED_QUANTITY(OBJ));
  send_to_char(CH, "q) Quit\r\nEnter your choice: ");
  d->edit_mode = AEDIT_MENU;
}

void create_ammo(struct char_data *ch)
{
  struct obj_data *ammo = read_object(OBJ_BLANK_AMMOBOX, VIRTUAL);
  STATE(ch->desc) = CON_AMMO_CREATE;
  GET_AMMOBOX_WEAPON(ammo) = WEAP_HOLDOUT;
  GET_AMMOBOX_INTENDED_QUANTITY(ammo) = AMMOBUILD_BATCH_SIZE;
  GET_AMMOBOX_QUANTITY(ammo) = 0;
  ch->desc->edit_obj = ammo;
  aedit_disp_menu(ch->desc);
}

void aedit_disp_weapon_menu(struct descriptor_data *d)
{
  CLS(CH);

  int index = 0;
  for (int counter = WEAP_HOLDOUT; counter < MAX_WEAP; counter++) {
    // We can't make cannon, grenade, or missile ammo here.
    if (counter >= WEAP_CANNON && counter != WEAP_MINIGUN)
      continue;

    index++;

    send_to_char(CH, "  %2d) %-18s%s",
                 index,
                 weapon_types[counter],
                 index % 2 == 0 || PRF_FLAGGED(CH, PRF_SCREENREADER) ? "\r\n" : ""
               );
  }
  send_to_char("Weapon type: ", d->character);
  d->edit_mode = AEDIT_WEAPON;
}

void aedit_disp_type_menu(struct descriptor_data *d)
{
  CLS(CH);
  for (int counter = 0; counter <= AMMO_HARMLESS; counter++)
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
      if (GET_AMMOBOX_WEAPON(OBJ) == WEAP_TASER) {
        send_to_char("Only normal taser darts are available.\r\n", CH);
        GET_AMMOBOX_TYPE(OBJ) = AMMO_NORMAL;
        aedit_disp_menu(d);
      } else {
        aedit_disp_type_menu(d);
      }
      break;
    case '3':
      send_to_char(CH,"Enter Quantity (in multiples of %d): ", AMMOBUILD_BATCH_SIZE);
      d->edit_mode = AEDIT_QUANTITY;
      break;
    case 'q':
      send_to_char(CH, "Ammo design saved!\r\n");
      snprintf(buf, sizeof(buf), "a box of %s %s ammunition", ammo_type[GET_AMMOBOX_TYPE(OBJ)].name, weapon_types[GET_AMMOBOX_WEAPON(OBJ)]);
      OBJ->restring = str_dup(buf);
      GET_AMMOBOX_CREATOR(OBJ) = GET_IDNUM(CH);
      obj_to_char(OBJ, CH);
      STATE(d) = CON_PLAYING;
      OBJ = NULL;
      break;
    }
    break;
  case AEDIT_TYPE:
   if (number > AMMO_HARMLESS || number < AMMO_NORMAL)
     send_to_char("Invalid selection.\r\nAmmo Type: ", CH);
   else {
     GET_AMMOBOX_TYPE(OBJ) = number;
     aedit_disp_menu(d);
   }
   break;
  case AEDIT_WEAPON:
    {
      int selected_weapon = WEAP_HOLDOUT, index = 0;
      for (selected_weapon = WEAP_HOLDOUT; selected_weapon < MAX_WEAP; selected_weapon++) {
        // We can't make cannon, grenade, or missile ammo here.
        if (selected_weapon >= WEAP_CANNON && selected_weapon != WEAP_MINIGUN)
          continue;

        if (++index == number)
          break;
      }

      if (number > index || selected_weapon >= MAX_WEAP)
        send_to_char("Invalid selection.\r\nWeapon Type: ", CH);
      else {
        GET_AMMOBOX_WEAPON(OBJ) = selected_weapon;
        GET_AMMOBOX_TYPE(OBJ) = AMMO_NORMAL;
        aedit_disp_menu(d);
      }
    }
    break;
  case AEDIT_QUANTITY:
    if (number < 0)
      number = 0;
    if (number > 1000)
      number = 1000;
    GET_AMMOBOX_INTENDED_QUANTITY(OBJ) = number - (number % AMMOBUILD_BATCH_SIZE);
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
    case WEAP_TASER:
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
    default:
      {
        char oopsbuf[500];
        snprintf(oopsbuf, sizeof(oopsbuf), "SYSERR: Unknown weapon type %d to gunsmith_skill!", weapon_type);
        mudlog(oopsbuf, NULL, LOG_SYSLOG, TRUE);
        return SKILL_BR_PISTOL;
      }
  }
}

bool ammo_test(struct char_data *ch, struct obj_data *obj)
{
  if (GET_NUYEN(ch) <= get_ammo_cost(GET_AMMOBOX_WEAPON(obj), GET_AMMOBOX_TYPE(obj)) * AMMOBUILD_BATCH_SIZE) {
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
  if (success > 0) {
    GET_AMMOBOX_TIME_TO_COMPLETION(obj) = (int)((ammo_type[GET_AMMOBOX_TYPE(obj)].time / MAX(success - csuccess, 1)) * 60);
    if (IS_SENATOR(ch)) {
      send_to_char(ch, "You use your staff powers to greatly accelerate the build time (was %d).\r\n", GET_AMMOBOX_TIME_TO_COMPLETION(obj));
      GET_AMMOBOX_TIME_TO_COMPLETION(obj) = 1;
    }
  } else
    GET_AMMOBOX_TIME_TO_COMPLETION(obj) = -1;
  lose_nuyen(ch, (int)((get_ammo_cost(GET_AMMOBOX_WEAPON(obj), GET_AMMOBOX_TYPE(obj)) * AMMOBUILD_BATCH_SIZE) * (1 - (csuccess * .05))), NUYEN_OUTFLOW_AMMO_BUILDING);
  GET_OBJ_VAL(obj, 10) = GET_AMMOBOX_TIME_TO_COMPLETION(obj);
  return TRUE;
}

void ammo_build(struct char_data *ch, struct obj_data *obj)
{
  if (GET_AMMOBOX_CREATOR(obj) != GET_IDNUM(ch)) {
    send_to_char(ch, "Looks like someone else already started on %s; they'll have to pick up where they left off.\r\n", decapitalize_a_an(GET_OBJ_NAME(obj)));
    return;
  }

  if (GET_AMMOBOX_INTENDED_QUANTITY(obj) <= 0) {
    send_to_char(ch, "%s has already been completed.", capitalize(GET_OBJ_NAME(obj)));
    return;
  }

  // Find a deployed workshop in the room.
  struct obj_data *workshop = find_workshop(ch, TYPE_AMMO);
  bool kitwarn = FALSE;

  // No workshop existed: Search for a kit in their inventory.
  if (!workshop) {
    for (workshop = ch->carrying; workshop; workshop = workshop->next_content) {
      if (GET_OBJ_TYPE(workshop) == ITEM_WORKSHOP && GET_WORKSHOP_TYPE(workshop) == TYPE_AMMO &&
          GET_WORKSHOP_GRADE(workshop) == TYPE_KIT) {
        // Ensure that, if a kit was found, the kit is of the right type.
        if (GET_AMMOBOX_TYPE(obj) == AMMO_NORMAL || GET_AMMOBOX_TYPE(obj) == AMMO_HARMLESS || (GET_AMMOBOX_TYPE(obj) == GET_WORKSHOP_AMMOKIT_TYPE(workshop)))
          break;
        else
          kitwarn = TRUE;
      }

    }
  }

  // No valid workshop / kit.
  if (!workshop) {
    if (kitwarn) {
      send_to_char(ch, "Your ammunition kit doesn't have the right tooling for %s ammo. You'll need a different kit or an ammunition workshop.\r\n",
                   ammo_type[GET_AMMOBOX_TYPE(obj)].name);
    } else {
      if (GET_AMMOBOX_TYPE(obj) != AMMO_NORMAL) {
        send_to_char(ch, "You need to be standing next to an unpacked ammunition workshop or to be carrying %s %s ammunition kit.\r\n",
                     AN(ammo_type[GET_AMMOBOX_TYPE(obj)].name),
                     ammo_type[GET_AMMOBOX_TYPE(obj)].name);
      } else {
        send_to_char("You need to be standing next to an unpacked ammunition workshop or to be carrying an ammunition kit.\r\n", ch);
      }
    }
    return;
  }

  if (GET_WORKSHOP_GRADE(workshop) == TYPE_KIT && GET_AMMOBOX_WEAPON(obj) == WEAP_CANNON) {
    send_to_char("You need an ammunition workshop to create assault cannon rounds.\r\n", ch);
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
