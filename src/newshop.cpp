#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <time.h>
#include <math.h>
#include <unistd.h>

#include "structs.h"
#include "awake.h"
#include "comm.h"
#include "handler.h"
#include "db.h"
#include "interpreter.h"
#include "utils.h"
#include "newshop.h"
#include "screen.h"
#include "olc.h"
#include "constants.h"
#include "config.h"
#include "newmail.h"

extern struct time_info_data time_info;
extern const char *pc_race_types[];

extern struct obj_data *get_first_credstick(struct char_data *ch, const char *arg);
extern void reduce_abilities(struct char_data *vict);
extern void do_probe_object(struct char_data * ch, struct obj_data * j);
extern void wire_nuyen(struct char_data *ch, int amount, vnum_t character_id);
ACMD_CONST(do_say);

bool can_sell_object(struct obj_data *obj, struct char_data *keeper, int shop_nr);
void shop_install(char *argument, struct char_data *ch, struct char_data *keeper, vnum_t shop_nr);
void shop_uninstall(char *argument, struct char_data *ch, struct char_data *keeper, vnum_t shop_nr);
struct obj_data *shop_package_up_ware(struct obj_data *obj);

int cmd_say;

const char *shop_flags[] =
  {
    "Nothing",
    "Doctor",
    "!NEGOTIATE",
    "!RESELL",
    "CHARGEN"
  };

const char *shop_type[3] =
  {
    "Grey",
    "Legal",
    "Black"
  };

const char *selling_type[] =
  {
    "Always",
    "Avail",
    "Stock",
    "Bought"
  };

bool is_open(struct char_data *keeper, int shop_nr)
{
#ifdef USE_SHOP_OPEN_CLOSE_TIMES
  char buf[MAX_STRING_LENGTH];
  buf[0] = '\0';
  if (shop_table[shop_nr].open > shop_table[shop_nr].close) {
    if (time_info.hours < shop_table[shop_nr].open && time_info.hours > shop_table[shop_nr].close)
      snprintf(buf, sizeof(buf), "We're not open yet.");
  } else {
    if (time_info.hours < shop_table[shop_nr].open)
      snprintf(buf, sizeof(buf), "We're not open yet.");
    else if (time_info.hours > shop_table[shop_nr].close)
      snprintf(buf, sizeof(buf), "We've closed for the day.");
  }
  if (!*buf)
    return TRUE;
  else
  {
    do_say(keeper, buf, cmd_say, 0);
    return FALSE;
  }
#else
  return TRUE;
#endif
}

bool is_ok_char(struct char_data * keeper, struct char_data * ch, vnum_t shop_nr)
{
  char buf[400];

  if (!access_level(ch, LVL_ADMIN) && !(CAN_SEE(keeper, ch))) {
    do_say(keeper, "I don't trade with someone I can't see.", cmd_say, 0);
    return FALSE;
  }
  if (IS_PROJECT(ch)) {
    send_to_char("You're having a hard time getting the shopkeeper's attention.\r\n", ch);
    return FALSE;
  }

  if (IS_NPC(ch) || access_level(ch, LVL_BUILDER)) {
    return TRUE;
  }

  if ((shop_table[shop_nr].races.IsSet(RACE_HUMAN) && GET_RACE(ch) == RACE_HUMAN) ||
      (shop_table[shop_nr].races.IsSet(RACE_ELF) && (GET_RACE(ch) == RACE_ELF ||
          GET_RACE(ch) == RACE_WAKYAMBI || GET_RACE(ch) == RACE_NIGHTONE ||
          GET_RACE(ch) == RACE_DRYAD)) ||
      (shop_table[shop_nr].races.IsSet(RACE_DWARF) && (GET_RACE(ch) == RACE_DWARF ||
          GET_RACE(ch) == RACE_KOBOROKURU || GET_RACE(ch) == RACE_MENEHUNE ||
          GET_RACE(ch) == RACE_GNOME)) ||
      (shop_table[shop_nr].races.IsSet(RACE_ORK) && (GET_RACE(ch) == RACE_ORK ||
          GET_RACE(ch) == RACE_ONI || GET_RACE(ch) == RACE_SATYR ||
          GET_RACE(ch) == RACE_HOBGOBLIN || GET_RACE(ch) == RACE_OGRE)) ||
      (shop_table[shop_nr].races.IsSet(RACE_TROLL) && (GET_RACE(ch) == RACE_TROLL ||
          GET_RACE(ch) == RACE_CYCLOPS || GET_RACE(ch) == RACE_GIANT ||
          GET_RACE(ch) == RACE_MINOTAUR || GET_RACE(ch) == RACE_FOMORI)))
  {
    snprintf(buf, sizeof(buf), "%s We don't sell to your type here.", GET_CHAR_NAME(ch));
    do_say(keeper, buf, cmd_say, SCMD_SAYTO);
    return FALSE;
  }
  return TRUE;
}

// Player buying from shop.
int buy_price(struct obj_data *obj, vnum_t shop_nr)
{
  // Base cost.
  int cost = GET_OBJ_COST(obj);

  // Multiply base cost by the shop's profit. Under no circumstances will we sell to them for less than 1x cost.
  cost = (int) round(cost * MAX(1, shop_table[shop_nr].profit_buy));

  // If the shop is black or grey market, multiply base cost by the item's street index.
  if (shop_table[shop_nr].type != SHOP_LEGAL && GET_OBJ_STREET_INDEX(obj) > 0)
    cost = (int) round(cost * GET_OBJ_STREET_INDEX(obj));

  // Add the random multiplier to the cost.
  cost += (int) round((cost * shop_table[shop_nr].random_current) / 100);

  // Enforce the final 1x cost requirement. This is an anti-exploit measure to prevent buying a cheap thing at shop A and reselling at shop B for profit.
  // Note that negotiation happens AFTER this, so people can still negotiate down.
  cost = MAX(cost, GET_OBJ_COST(obj));

  // Return the final value.
  return cost;
}

// Player selling to shop.
int sell_price(struct obj_data *obj, vnum_t shop_nr)
{
  // Base cost.
  int cost = (int) round(GET_OBJ_COST(obj) * shop_table[shop_nr].profit_sell);

  // If the street index is set but is less than 1, multiply by this index regardless of shop legality.
  // This fixes an exploit where someone could buy a discounted thing at a black/grey shop and sell to a legal one for a profit.
  if (GET_OBJ_STREET_INDEX(obj) > 0 && GET_OBJ_STREET_INDEX(obj) < 1)
    cost = (int) round(cost * GET_OBJ_STREET_INDEX(obj));

  // Add the random multiplier to the cost.
  cost += (int) round((cost * shop_table[shop_nr].random_current) / 100);

  return cost;
}

int transaction_amt(char *arg, size_t arg_len)
{
  int num;
  one_argument(arg, buf);
  if (*buf)
    if ((is_number(buf))) {
      num = atoi(buf);
      char temp_buf[strlen(arg)];
      strlcpy(temp_buf, arg + strlen(buf) + 1, sizeof(temp_buf));
      strlcpy(arg, temp_buf, arg_len);
      return (num);
    }
  return (1);
}

struct shop_sell_data *find_obj_shop(char *arg, vnum_t shop_nr, struct obj_data **obj)
{
  *obj = NULL;
  struct shop_sell_data *sell = shop_table[shop_nr].selling;
  if (*arg == '#' && atoi(arg+1) > 0)
  {
    int num = atoi(arg+1);
    for (;sell; sell = sell->next) {
      num--;

      int real_obj = real_object(sell->vnum);

      if (real_obj >= 0) {
        // Can't sell it? Don't have it show up here.
        struct obj_data *temp_obj = read_object(real_obj, REAL);
        if (!can_sell_object(temp_obj, NULL, shop_nr)) {
          num++;
          continue;
        }
        extract_obj(temp_obj);
        temp_obj = NULL;
        if (num <= 0)
          break;
      } else {
        num++;
        continue;
      }
    }
    if (sell)
      *obj = read_object(sell->vnum, VIRTUAL);
  } else
  {
    for (; sell; sell = sell->next) {
      int real_obj = real_object(sell->vnum);
      if (real_obj >= 0) {
        if (obj_proto[real_obj].obj_flags.cost &&
            (isname(arg, obj_proto[real_obj].text.name) ||
             isname(arg, obj_proto[real_obj].text.keywords))) {
          *obj = read_object(sell->vnum, VIRTUAL);
          break;
        }
      }
    }
  }
  return sell;
}

bool uninstall_ware_from_target_character(struct obj_data *obj, struct char_data *remover, struct char_data *victim, bool damage_on_operation) {
  if (remover == victim) {
    send_to_char(remover, "You can't operate on yourself!\r\n");
    mudlog("SYSERR: remover = victim in uninstall_ware_from_target_character(). That's not supposed to happen!", remover, LOG_SYSLOG, TRUE);
    return FALSE;
  }

  if (GET_OBJ_TYPE(obj) != ITEM_BIOWARE && GET_OBJ_TYPE(obj) != ITEM_CYBERWARE) {
    snprintf(buf3, sizeof(buf3), "SYSERR: Non-ware object '%s' (%ld) passed to uninstall_ware_from_target_character()!", GET_OBJ_NAME(obj), GET_OBJ_VNUM(obj));
    mudlog(buf3, remover, LOG_SYSLOG, TRUE);
    send_to_char(remover, "An unexpected error occurred when trying to uninstall %s.\r\n", GET_OBJ_NAME(obj));
    return FALSE;
  }

  if (GET_OBJ_COST(obj) == 0 && !IS_NPC(remover)) {
    send_to_char(remover, "%s is Chargen 'ware, so it can't be removed by player cyberdocs. Have your patient sell it to an NPC doc.\r\n", capitalize(GET_OBJ_NAME(obj)));
    return FALSE;
  }

  if (GET_OBJ_TYPE(obj) == ITEM_BIOWARE) {
    obj_from_bioware(obj);
    GET_INDEX(victim) -= GET_CYBERWARE_ESSENCE_COST(obj);
    GET_INDEX(victim) = MAX(0, GET_INDEX(victim));
  } else {
    obj_from_cyberware(obj);
    GET_ESSHOLE(victim) += GET_CYBERWARE_ESSENCE_COST(obj);
  }

  if (!IS_NPC(remover)) {
    snprintf(buf, sizeof(buf), "Player Cyberdoc: %s uninstalled %s from %s.", GET_CHAR_NAME(remover), GET_OBJ_NAME(obj), GET_CHAR_NAME(victim));
    mudlog(buf, remover, LOG_GRIDLOG, TRUE);
  }

  act("$n takes out a sharpened scalpel and lies $N down on the operating table.",
      FALSE, remover, 0, victim, TO_NOTVICT);
  if (IS_NPC(remover)) {
    snprintf(buf, sizeof(buf), "%s Relax...this won't hurt a bit.", GET_CHAR_NAME(victim));
    do_say(remover, buf, cmd_say, SCMD_SAYTO);
  }
  act("You delicately remove $p from $N's body.",
      FALSE, remover, obj, victim, TO_CHAR);
  act("$n performs a delicate procedure on $N.",
      FALSE, remover, 0, victim, TO_NOTVICT);
  act("$n delicately removes $p from your body.",
      FALSE, remover, obj, victim, TO_VICT);

  // If this isn't a newbie shop, damage them like they're just coming out of surgery.
  if (damage_on_operation) {
    GET_PHYSICAL(victim) = 100;
    GET_MENTAL(victim) = 100;
  }

  affect_total(victim);
  if (GET_OBJ_TYPE(obj) == ITEM_CYBERWARE && GET_CYBERWARE_TYPE(obj) == CYB_MEMORY)
    for (struct obj_data *chip = obj->contains; chip; chip = chip->next_content)
      if (GET_OBJ_VAL(chip, 9))
        victim->char_specials.saved.skills[GET_OBJ_VAL(chip, 0)][1] = 0;

  return TRUE;
}

bool install_ware_in_target_character(struct obj_data *ware, struct char_data *installer, struct char_data *recipient, bool damage_on_operation) {
  struct obj_data *check;

  if (installer == recipient) {
    send_to_char(installer, "You can't operate on yourself!\r\n");
    mudlog("SYSERR: installer = recipient in install_ware_in_target_character(). That's not supposed to happen!", installer, LOG_SYSLOG, TRUE);
    return FALSE;
  }

  strlcpy(buf, GET_CHAR_NAME(recipient), sizeof(buf));

  // Item must be compatible with your current gear.
  switch (GET_OBJ_TYPE(ware)) {
    case ITEM_CYBERWARE:
    case ITEM_BIOWARE:
      for (struct obj_data *bio = recipient->bioware; bio; bio = bio->next_content)
        if (!biocyber_compatibility(ware, bio, recipient)) {
          send_to_char(installer, "That 'ware isn't compatible with what's already installed.\r\n");
          return FALSE;
        }
      for (struct obj_data *cyber = recipient->cyberware; cyber; cyber = cyber->next_content)
        if (!biocyber_compatibility(ware, cyber, recipient)) {
          send_to_char(installer, "That 'ware isn't compatible with what's already installed.\r\n");
          return FALSE;
        }
      break;
    default:
      snprintf(buf3, sizeof(buf3), "SYSERR: Non-ware object '%s' (%ld) passed to install_ware_in_target_character()!", GET_OBJ_NAME(ware), GET_OBJ_VNUM(ware));
      mudlog(buf3, installer, LOG_SYSLOG, TRUE);
      send_to_char(installer, "An unexpected error occurred when trying to install %s (code 1.\r\n", GET_OBJ_NAME(ware));
      send_to_char(recipient, "An unexpected error occurred when trying to install %s (code 1).\r\n", GET_OBJ_NAME(ware));
      return FALSE;
  }

  // Edge case: We remove the object from its container further down, and we want to make sure this doesn't break anything.
  if (ware->in_obj && GET_OBJ_TYPE(ware->in_obj) != ITEM_SHOPCONTAINER) {
    snprintf(buf3, sizeof(buf3), "SYSERR: '%s' (%ld) contained in something that's not a shopcontainer!", GET_OBJ_NAME(ware), GET_OBJ_VNUM(ware));
    mudlog(buf3, installer, LOG_SYSLOG, TRUE);
    send_to_char(installer, "An unexpected error occurred when trying to install %s (code 2).\r\n", GET_OBJ_NAME(ware));
    send_to_char(recipient, "An unexpected error occurred when trying to install %s (code 2).\r\n", GET_OBJ_NAME(ware));
    return FALSE;
  }

  // Don't shrek the mages.
  if (IS_OBJ_STAT(ware, ITEM_MAGIC_INCOMPATIBLE) && (GET_MAG(recipient) > 0 || GET_TRADITION(recipient) != TRAD_MUNDANE)) {
    if (IS_NPC(installer)) {
      snprintf(ENDOF(buf), sizeof(buf) - strlen(buf), " That operation would eradicate your magic!");
      do_say(installer, buf, cmd_say, SCMD_SAYTO);
    } else {
      send_to_char(installer, "You can't install %s-- it's not compatible with magic.\r\n", GET_OBJ_NAME(ware));
    }
    return FALSE;
  }

  // Reject installing magic-incompat 'ware into magic-using characters.
  if (GET_OBJ_TYPE(ware) == ITEM_CYBERWARE) {
    int esscost = GET_CYBERWARE_ESSENCE_COST(ware);
    if (GET_TOTEM(recipient) == TOTEM_EAGLE)
      esscost *= 2;

    // Check to see if the operation is even possible with their current essence / hole.
    if (GET_REAL_ESS(recipient) + GET_ESSHOLE(recipient) < esscost) {
      if (IS_NPC(installer)) {
        snprintf(ENDOF(buf), sizeof(buf) - strlen(buf), " That operation would kill you!");
        do_say(installer, buf, cmd_say, SCMD_SAYTO);
      } else {
        send_to_char(installer, "There's not enough meat left to install %s into!\r\n", GET_OBJ_NAME(ware));
      }
      return FALSE;
    }

    // Check for matching cyberware and related limits.
    int enhancers = 0;
    for (check = recipient->cyberware; check != NULL; check = check->next_content) {
      if ((GET_OBJ_VNUM(check) == GET_OBJ_VNUM(ware))) {
        if (GET_CYBERWARE_TYPE(check) == CYB_REACTIONENHANCE) {
          if (++enhancers == 6) {
            if (IS_NPC(installer)) {
              snprintf(ENDOF(buf), sizeof(buf) - strlen(buf), " You already have the maximum number of reaction enhancers installed.");
              do_say(installer, buf, cmd_say, SCMD_SAYTO);
            } else {
              send_to_char(installer, "You can't install %s-- more reaction enhancers wouldn't have any effect.\r\n", GET_OBJ_NAME(ware));
            }
            return FALSE;
          }
        } else {
          if (IS_NPC(installer)) {
            snprintf(ENDOF(buf), sizeof(buf) - strlen(buf), " You already have %s installed.", GET_OBJ_NAME(ware));
            do_say(installer, buf, cmd_say, SCMD_SAYTO);
          } else {
            send_to_char(installer, "You can't install %s-- another one is already installed.\r\n", GET_OBJ_NAME(ware));
          }
          return FALSE;
        }
      }
      if (GET_CYBERWARE_TYPE(check) == GET_CYBERWARE_TYPE(ware)
          && (GET_CYBERWARE_TYPE(ware) != CYB_EYES && GET_CYBERWARE_TYPE(ware) != CYB_FILTRATION && GET_CYBERWARE_TYPE(ware) != CYB_REACTIONENHANCE))
      {
        if (IS_NPC(installer)) {
          snprintf(ENDOF(buf), sizeof(buf) - strlen(buf), " You already have %s, and it's too similar to %s for them to work together.", GET_OBJ_NAME(check), GET_OBJ_NAME(ware));
          do_say(installer, buf, cmd_say, SCMD_SAYTO);
        } else {
          send_to_char(installer, "You can't install %s-- it's too similar to the already-installed %s.\r\n", GET_OBJ_NAME(ware), GET_OBJ_NAME(check));
        }
        return FALSE;
      }
    }

    // Adapt for essence hole.
    if (GET_ESSHOLE(recipient) < esscost) {
      esscost = esscost - GET_ESSHOLE(recipient);

      // Deduct magic, if any.
      if (GET_TRADITION(recipient) != TRAD_MUNDANE) {
        if (GET_REAL_MAG(recipient) - esscost < 100) {
          if (IS_NPC(installer)) {
            snprintf(ENDOF(buf), sizeof(buf) - strlen(buf), " That would take away the last of your magic!");
            do_say(installer, buf, cmd_say, SCMD_SAYTO);
          } else {
            send_to_char(installer, "%s would take away the last of their magic!\r\n", capitalize(GET_OBJ_NAME(ware)));
          }
          return FALSE;
        }
        magic_loss(recipient, esscost, TRUE);
      }
      GET_ESSHOLE(recipient) = 0;
      GET_REAL_ESS(recipient) -= esscost;
    } else {
      GET_ESSHOLE(recipient) -= esscost;
    }

    // Unpackage it if needed, and extract the container.
    if (ware->in_obj && GET_OBJ_TYPE(ware->in_obj) == ITEM_SHOPCONTAINER) {
      struct obj_data *container = ware->in_obj;
      obj_from_obj(ware);
      extract_obj(container);
      container = NULL;
    }

    // Install it.
    obj_to_cyberware(ware, recipient);
  }

  // You must have the index to support it.
  else if (GET_OBJ_TYPE(ware) == ITEM_BIOWARE) {
    int esscost = GET_BIOWARE_ESSENCE_COST(ware);
    if (GET_INDEX(recipient) + esscost > 900) {
      if (IS_NPC(installer)) {
        snprintf(ENDOF(buf), sizeof(buf) - strlen(buf), " That operation would kill you!");
        do_say(installer, buf, cmd_say, SCMD_SAYTO);
      } else {
        send_to_char(installer, "There's not enough meat left to install %s into!\r\n", GET_OBJ_NAME(ware));
      }
      return FALSE;
    }

    if ((GET_BIOWARE_TYPE(ware) == BIO_PATHOGENICDEFENSE || GET_BIOWARE_TYPE(ware) == BIO_TOXINEXTRACTOR) &&
        GET_BIOWARE_RATING(ware) > GET_REAL_BOD(recipient) / 2)
    {
      if (IS_NPC(installer)) {
        snprintf(ENDOF(buf), sizeof(buf) - strlen(buf), " Your body can't support pathogenic defenses that are that strong.");
        do_say(installer, buf, cmd_say, SCMD_SAYTO);
      } else {
        send_to_char(installer, "The defenses from %s are too powerful for their body.\r\n", GET_OBJ_NAME(ware));
      }
      return FALSE;
    }

    for (check = recipient->bioware; check; check = check->next_content) {
      if ((GET_OBJ_VNUM(check) == GET_OBJ_VNUM(ware))) {
        if (IS_NPC(installer)) {
          snprintf(ENDOF(buf), sizeof(buf) - strlen(buf), " You already have that installed.");
          do_say(installer, buf, cmd_say, SCMD_SAYTO);
        } else {
          send_to_char(installer, "Another %s is already installed.\r\n", GET_OBJ_NAME(ware));
        }
        return FALSE;
      }
      if (GET_BIOWARE_TYPE(check) == GET_BIOWARE_TYPE(ware)) {
        if (IS_NPC(installer)) {
          snprintf(ENDOF(buf), sizeof(buf) - strlen(buf), " You already have %s installed, and it's too similar to %s for them to work together.", GET_OBJ_NAME(check), GET_OBJ_NAME(ware));
          do_say(installer, buf, cmd_say, SCMD_SAYTO);
        } else {
          send_to_char(installer, "You can't install %s-- the already-installed %s would conflict with it.\r\n", GET_OBJ_NAME(ware), GET_OBJ_NAME(check));
        }
        return FALSE;
      }
    }

    GET_INDEX(recipient) += esscost;
    if (GET_INDEX(recipient) > recipient->real_abils.highestindex) {
      if (GET_TRADITION(recipient) != TRAD_MUNDANE) {
        int change = GET_INDEX(recipient) - recipient->real_abils.highestindex;
        change /= 2;
        if (GET_REAL_MAG(recipient) - change < 100) {
          if (IS_NPC(installer)) {
            snprintf(ENDOF(buf), sizeof(buf) - strlen(buf), " That would take away the last of your magic!");
            do_say(installer, buf, cmd_say, SCMD_SAYTO);
          } else {
            send_to_char(installer, "You can't install %s-- it would take away the last of their magic.\r\n", GET_OBJ_NAME(ware), GET_OBJ_NAME(check));
          }
          do_say(installer, buf, cmd_say, SCMD_SAYTO);
          GET_INDEX(recipient) -= esscost;
          return FALSE;
        }
        magic_loss(recipient, change, TRUE);
      }
      recipient->real_abils.highestindex = GET_INDEX(recipient);
    }

    // Unpackage it if needed, and extract the container.
    if (ware->in_obj && GET_OBJ_TYPE(ware->in_obj) == ITEM_SHOPCONTAINER) {
      struct obj_data *container = ware->in_obj;
      obj_from_obj(ware);
      extract_obj(container);
      container = NULL;
    }

    // Install it.
    obj_to_bioware(ware, recipient);
  }

  if (!IS_NPC(installer)) {
    snprintf(buf, sizeof(buf), "Player Cyberdoc: %s installed %s in %s.", GET_CHAR_NAME(installer), GET_OBJ_NAME(ware), GET_CHAR_NAME(recipient));
    mudlog(buf, installer, LOG_GRIDLOG, TRUE);
  }

  // Send installation messages.
  act("$n takes out a sharpened scalpel and lies $N down on the operating table.",
      FALSE, installer, 0, recipient, TO_NOTVICT);
  if (IS_NPC(installer)) {
    snprintf(buf, sizeof(buf), "%s Relax...this won't hurt a bit.", GET_CHAR_NAME(recipient));
    do_say(installer, buf, cmd_say, SCMD_SAYTO);
  }
  act("You delicately install $p into $N's body.",
      FALSE, installer, ware, recipient, TO_CHAR);
  act("$n performs a delicate procedure on $N.",
      FALSE, installer, 0, recipient, TO_NOTVICT);
  act("$n delicately installs $p into your body.",
      FALSE, installer, ware, recipient, TO_VICT);

  // If this isn't a newbie shop, damage them like they're just coming out of surgery.
  if (damage_on_operation) { // aka !shop_table[shop_nr].flags.IsSet(SHOP_CHARGEN)
    GET_PHYSICAL(recipient) = 100;
    GET_MENTAL(recipient) = 100;
  }

  if (GET_BIOOVER(recipient) > 0)
    send_to_char("You don't feel too well.\r\n", recipient);
  return TRUE;
}

// Yes, it's a monstrosity. No, I don't want to hear about it. YOU refactor it.
bool shop_receive(struct char_data *ch, struct char_data *keeper, char *arg, int buynum, bool cash,
                  struct shop_sell_data *sell, struct obj_data *obj, struct obj_data *cred, int price,
                  vnum_t shop_nr, struct shop_order_data *order)
{
  char buf[MAX_STRING_LENGTH], buf2[MAX_STRING_LENGTH];
  strcpy(buf, GET_CHAR_NAME(ch));
  int bought = 0;
  bool print_multiples_at_end = TRUE;

  // Item must be available in the store.
  if (sell && sell->type == SELL_STOCK && sell->stock < 1)
  {
    snprintf(ENDOF(buf), sizeof(buf) - strlen(buf), " That item isn't currently available.");
    do_say(keeper, buf, cmd_say, SCMD_SAYTO);
    return FALSE;
  }

  // Character must have enough nuyen for it.
  if ((cred && GET_ITEM_MONEY_VALUE(cred) < price) || (!cred && GET_NUYEN(ch) < price))
  {
    snprintf(ENDOF(buf), sizeof(buf) - strlen(buf), " %s", shop_table[shop_nr].not_enough_nuyen);
    do_say(keeper, buf, cmd_say, SCMD_SAYTO);
    return FALSE;
  }

  // Pre-compose our string.
  snprintf(buf2, sizeof(buf2), "You now have %s.", GET_OBJ_NAME(obj));

  // Cyberware / bioware doctor.
  if (shop_table[shop_nr].flags.IsSet(SHOP_DOCTOR)) {
    if (!shop_table[shop_nr].flags.IsSet(SHOP_CHARGEN)) {
      // We used to have a ton of compatibility checks here, but now we just give them the thing in a box!
      struct obj_data *shop_container = shop_package_up_ware(obj);
      act("$n packages up $N's purchase and hands it to $M.", TRUE, keeper, NULL, ch, TO_NOTVICT);
      act("$n packages up your purchase and hands it to you.", TRUE, keeper, NULL, ch, TO_VICT);
      obj_to_char(shop_container, ch);

      snprintf(buf2, sizeof(buf2), "You now have %s.", GET_OBJ_NAME(shop_container));

      // I'm tempted to reduce the price by the installation cost, but that brings up all sorts of exploits and edge cases.
      // For example, if install cost is 10k and the thing cost 9k, it's free to purchase, and you can sell it back for a ~3k profit!
    } else {
      if (!install_ware_in_target_character(obj, keeper, ch, FALSE))
        return FALSE;
    }

    bought = 1;

    if (cred)
      lose_nuyen_from_credstick(ch, cred, price, NUYEN_OUTFLOW_SHOP_PURCHASES);
    else
      lose_nuyen(ch, price, NUYEN_OUTFLOW_SHOP_PURCHASES);

    // Log it.
    snprintf(buf, sizeof(buf), "Purchased cyber/bio '%s' (%ld) for %d nuyen.", GET_OBJ_NAME(obj), GET_OBJ_VNUM(obj), price);
    mudlog(buf, ch, LOG_GRIDLOG, TRUE);

    if (sell) {
      if (sell->type == SELL_BOUGHT && !--sell->stock) {
        struct shop_sell_data *temp;
        REMOVE_FROM_LIST(sell, shop_table[shop_nr].selling, next);
        delete sell;
        sell = NULL;
      } else if (sell->type == SELL_STOCK)
        sell->stock--;
    }
  }

  // Neither cyber nor bioware. Handle as normal object.
  else {
    if (IS_CARRYING_N(ch) + 1 > CAN_CARRY_N(ch)) {
      send_to_char("You can't carry any more items.\r\n", ch);
      return FALSE;
    }
    if (IS_CARRYING_W(ch) + GET_OBJ_WEIGHT(obj) > CAN_CARRY_W(ch)) {
      send_to_char("It weighs too much!\r\n", ch);
      return FALSE;
    }

    // Special handling for stackable things. TODO: Review this to make sure sell struct etc is updated appropriately.
    if ((GET_OBJ_TYPE(obj) == ITEM_DECK_ACCESSORY && GET_OBJ_VAL(obj, 0) == TYPE_PARTS)
        || (GET_OBJ_TYPE(obj) == ITEM_MAGIC_TOOL && GET_OBJ_VAL(obj, 0) == TYPE_SUMMONING)
        || (GET_OBJ_TYPE(obj) == ITEM_GUN_AMMO))
    {
      bought = 0;
      float current_obj_weight = 0;

      // Deduct money up to the amount they can afford. Update the object's cost to match.
      while (bought < buynum && (cred ? GET_ITEM_MONEY_VALUE(cred) : GET_NUYEN(ch)) >= price) {
        bought++;

        // Prevent taking more than you can carry.
        current_obj_weight += GET_OBJ_WEIGHT(obj);
        if (IS_CARRYING_W(ch) + current_obj_weight > CAN_CARRY_W(ch)) {
          if (--bought <= 0) {
            send_to_char("It weighs too much.\r\n", ch);
            return FALSE;
          } else {
            // send_to_char(ch, "You can only carry %d of that.\r\n", bought);
            // ^-- the shopkeeper will tell them this.
            break;
          }
        }

        if (cred)
          lose_nuyen_from_credstick(ch, cred, price, NUYEN_OUTFLOW_SHOP_PURCHASES);
        else
          lose_nuyen(ch, price, NUYEN_OUTFLOW_SHOP_PURCHASES);
      }

      if (bought == 0) {
        send_to_char("You can't afford that.\r\n", ch);
        return FALSE;
      }

      GET_OBJ_COST(obj) = GET_OBJ_COST(obj) * bought;

      // Give them the item (it's gun ammo)
      if (GET_OBJ_TYPE(obj) == ITEM_GUN_AMMO) {
        print_multiples_at_end = FALSE;

        // Update its quantity and weight to match the increased ammo load. Cost already done above.
        GET_AMMOBOX_QUANTITY(obj) *= bought;
        GET_OBJ_WEIGHT(obj) *= bought;

        // In theory this is dead code now after the 'you can only carry x' code change above. Will see.
        if (IS_CARRYING_W(ch) + GET_OBJ_WEIGHT(obj) > CAN_CARRY_W(ch)) {
          send_to_char("You start gathering up the ammo you paid for, but realize you can't carry it all! The shopkeeper gives you a /look/, then refunds you in cash.\r\n", ch);
          // In this specific instance, we not only assign raw nuyen, we also decrement the purchase nuyen counter. It's a refund, after all.
          long refund_amount = price * bought;
          GET_NUYEN_RAW(ch) += refund_amount;
          GET_NUYEN_INCOME_THIS_PLAY_SESSION(ch, NUYEN_OUTFLOW_SHOP_PURCHASES) -= refund_amount;
          extract_obj(obj);
          return FALSE;
        }

        struct obj_data *orig = ch->carrying;
        for (; orig; orig = orig->next_content) {
          if (GET_OBJ_TYPE(obj) == ITEM_GUN_AMMO &&
              GET_AMMOBOX_INTENDED_QUANTITY(obj) <= 0 &&
              GET_AMMOBOX_WEAPON(obj) == GET_AMMOBOX_WEAPON(orig) &&
              GET_AMMOBOX_TYPE(obj) == GET_AMMOBOX_TYPE(orig))
            break;
        }
        if (orig) {
          // They were carrying one already. Combine them.
          snprintf(buf2, sizeof(buf2), "You add the purchased %d rounds", GET_AMMOBOX_QUANTITY(obj));
          combine_ammo_boxes(ch, obj, orig, FALSE);
          snprintf(ENDOF(buf2), sizeof(buf2) - strlen(buf2), " into %s.", GET_OBJ_NAME(orig));
        } else {
          // Just give the purchased thing to them directly. Handle restring if needed.
          if (bought > 1) {
            char new_name_buf[500];

            // Compose the new name.
            snprintf(new_name_buf, sizeof(new_name_buf), "a box of %s %s ammunition",
              ammo_type[GET_AMMOBOX_TYPE(obj)].name,
              weapon_type[GET_AMMOBOX_WEAPON(obj)]
            );

            // Commit the change.
            obj->restring = str_dup(new_name_buf);

            snprintf(buf2, sizeof(buf2), "You now have %s (contains %d rounds).", GET_OBJ_NAME(obj), GET_AMMOBOX_QUANTITY(obj));
          } else {
            snprintf(buf2, sizeof(buf2), "You now have %s.", GET_OBJ_NAME(obj));
          }
          // buf2 is sent to the character with a newline appended at the end of the function.

          obj_to_char(obj, ch);
        }
        // TODO: Handle decrementing shop amounts here (->stock). Currently, shop items are not decremented on sale for these item types.
      }

      // Give them the item (it's parts or conjuring materials)
      else {
        struct obj_data *orig = ch->carrying;
        for (; orig; orig = orig->next_content)
          if (GET_OBJ_TYPE(obj) == GET_OBJ_TYPE(orig)
              && GET_OBJ_VAL(obj, 0) == GET_OBJ_VAL(orig, 0)
              && GET_OBJ_VAL(obj, 1) == GET_OBJ_VAL(orig, 1))
            break;
        if (orig) {
          GET_OBJ_COST(orig) += GET_OBJ_COST(obj);
          extract_obj(obj);
          obj = NULL;
        } else {
          obj_to_char(obj, ch);
        }
      }
    }

    // Non-stackable things.
    else {
      while (obj && (bought < buynum
                     && IS_CARRYING_N(ch) < CAN_CARRY_N(ch)
                     && IS_CARRYING_W(ch) + GET_OBJ_WEIGHT(obj) <= CAN_CARRY_W(ch)
                     && (cred ? GET_ITEM_MONEY_VALUE(cred) : GET_NUYEN(ch)) >= price)) {
        // Visas are ID-locked to the purchaser.
        if (GET_OBJ_VNUM(obj) == OBJ_MULTNOMAH_VISA || GET_OBJ_VNUM(obj) == OBJ_CARIBBEAN_VISA)
          GET_OBJ_VAL(obj, 0) = GET_IDNUM(ch);

        obj_to_char(obj, ch);
        bought++;

        if (sell) {
          obj = NULL;
          switch (sell->type) {
            case SELL_BOUGHT:
              if (--(sell->stock) == 0) {
                struct shop_sell_data *temp = NULL;
                REMOVE_FROM_LIST(sell, shop_table[shop_nr].selling, next);
                DELETE_AND_NULL(sell);
              } else
                obj = read_object(sell->vnum, VIRTUAL);
              break;
            case SELL_STOCK:
              sell->stock--;
              if (sell->stock > 0)
                obj = read_object(sell->vnum, VIRTUAL);
              break;
            default:
              obj = read_object(sell->vnum, VIRTUAL);
              break;
          }
        } else {
          obj = read_object(obj->item_number, REAL);
        }

        // Deduct the cost.
        if (cred)
          lose_nuyen_from_credstick(ch, cred, price, NUYEN_OUTFLOW_SHOP_PURCHASES);
        else
          lose_nuyen(ch, price, NUYEN_OUTFLOW_SHOP_PURCHASES);
      }
      if (obj) {
        // Obj was loaded but not given to the character.
        extract_obj(obj);
        obj = NULL;
      }
    }

    if (bought < buynum) {
      strcpy(buf, GET_CHAR_NAME(ch));
      if (IS_CARRYING_N(ch) >= CAN_CARRY_N(ch))
        snprintf(ENDOF(buf), sizeof(buf) - strlen(buf), " You can only carry %d.", bought);
      else if (GET_OBJ_WEIGHT(ch->carrying) + IS_CARRYING_W(ch) > CAN_CARRY_W(ch))
        snprintf(ENDOF(buf), sizeof(buf) - strlen(buf), " You can only carry %d.", bought);
      else if ((cash ? GET_NUYEN(ch) : GET_ITEM_MONEY_VALUE(cred)) < price)
        snprintf(ENDOF(buf), sizeof(buf) - strlen(buf), " You can only afford %d.", bought);
      else
        snprintf(ENDOF(buf), sizeof(buf) - strlen(buf), " I'm only willing to give you %d.", bought); // Error case.
      do_say(keeper, buf, cmd_say, SCMD_SAYTO);
    }
  }
  // Write the nuyen cost to buf3 and the current buy-string to arg.
  char price_buf[100];
  snprintf(price_buf, sizeof(price_buf), "%d", bought * price);
  strcpy(arg, shop_table[shop_nr].buy);

  // Use our new replace_substring() function to swap out all %d's in arg with the nuyen string.
  replace_substring(arg, buf3, "%d", price_buf);

  // Compose the sayto string for the keeper.
  snprintf(buf, sizeof(buf), "%s %s", GET_CHAR_NAME(ch), buf3);
  do_say(keeper, buf, cmd_say, SCMD_SAYTO);
  if (bought > 1 && print_multiples_at_end)
    snprintf(ENDOF(buf2), sizeof(buf2) - strlen(buf2), " (x%d)", bought);
  send_to_char(buf2, ch);
  send_to_char("\r\n", ch);

  // Log it. Right now, this prints a null object most of the time.
  /*
  if (bought >= 1 && obj) {
    snprintf(buf, sizeof(buf), "Purchased %d of '%s' (%ld) for %d nuyen.", bought, GET_OBJ_NAME(obj), GET_OBJ_VNUM(obj), price);
    mudlog(buf, ch, LOG_GRIDLOG, TRUE);
  }
  */

  if (order) {
    order->number -= bought;
    if (order->number == 0) {
      struct shop_order_data *temp;
      REMOVE_FROM_LIST(order, shop_table[shop_nr].order, next);
      delete order;
    } else if (order->number < 0) {
      mudlog("SYSERR: Purchased quantity greater than ordered quantity in shop_receive()!", ch, LOG_SYSLOG, TRUE);
    }
  }

  return TRUE;
}

void shop_buy(char *arg, size_t arg_len, struct char_data *ch, struct char_data *keeper, vnum_t shop_nr)
{
  char buf[MAX_STRING_LENGTH];
  if (!is_open(keeper, shop_nr))
    return;
  if (!is_ok_char(keeper, ch, shop_nr))
    return;
  struct obj_data *obj, *cred = get_first_credstick(ch, "credstick");
  struct shop_sell_data *sell;
  int price, buynum;
  bool cash = FALSE;

  // Prevent negative transactions.
  if ((buynum = transaction_amt(arg, arg_len)) < 0)
  {
    snprintf(buf, sizeof(buf), "%s A negative amount?  Try selling me something.", GET_CHAR_NAME(ch));
    do_say(keeper, buf, cmd_say, SCMD_SAYTO);
    return;
  }

  // Find the item in their list.
  if (!(sell = find_obj_shop(arg, shop_nr, &obj)))
  {
    if (atoi(arg) > 0) {
      // Adapt for the player probably meaning an item number instead of an item with a numeric keyword.
      char oopsbuf[strlen(arg) + 2];
      snprintf(oopsbuf, sizeof(oopsbuf), "#%s", arg);
      sell = find_obj_shop(oopsbuf, shop_nr, &obj);
    }
    if (!sell) {
      snprintf(buf, sizeof(buf), "%s %s", GET_CHAR_NAME(ch), shop_table[shop_nr].no_such_itemk);
      do_say(keeper, buf, cmd_say, SCMD_SAYTO);
      return;
    }
  }

  one_argument(arg, buf);

  // Allow specification of cash purchases in grey shops.
  if (!str_cmp(buf, "cash"))
  {
    if (shop_table[shop_nr].type == SHOP_LEGAL) {
      if (access_level(ch, LVL_ADMIN)) {
        send_to_char(ch, "You stare unblinkingly at %s until %s makes an exception to the no-credstick, no-sale policy.\r\n",
                     GET_NAME(keeper), HSSH(keeper));
      } else {
        snprintf(buf, sizeof(buf), "%s No Credstick, No Sale.", GET_CHAR_NAME(ch));
        do_say(keeper, buf, cmd_say, SCMD_SAYTO);
        return;
      }
    }

    arg = any_one_arg(arg, buf);
    skip_spaces(&arg);
    cash = TRUE;
  }

  // Fallback: You didn't specify cash, and you have no credstick on hand.
  else if (!cred)
  {
    if (shop_table[shop_nr].type == SHOP_LEGAL) {
      if (access_level(ch, LVL_ADMIN)) {
        send_to_char(ch, "You stare unblinkingly at %s until %s makes an exception to the no-credstick, no-sale policy.\r\n",
                     GET_NAME(keeper), HSSH(keeper));
      } else {
        snprintf(buf, sizeof(buf), "%s No Credstick, No Sale.", GET_CHAR_NAME(ch));
        do_say(keeper, buf, cmd_say, SCMD_SAYTO);
        return;
      }
    }
    send_to_char("Lacking an activated credstick, you choose to deal in cash.\r\n", ch );
    cash = TRUE;
  }

  // You have a credstick, but the shopkeeper doesn't want it.
  else if (shop_table[shop_nr].type == SHOP_BLACK)
  {
    send_to_char("The shopkeeper refuses to deal with credsticks.\r\n", ch);
    cash = TRUE;
  }

  // You must clarify what you want to buy.
  if (!*arg || !buynum)
  {
    snprintf(buf, sizeof(buf), "%s What do you want to buy?", GET_CHAR_NAME(ch));
    do_say(keeper, buf, cmd_say, SCMD_SAYTO);
    return;
  }

  // Calculate the price.
  price = buy_price(obj, shop_nr);
  int bprice = price / 10;
  if (!shop_table[shop_nr].flags.IsSet(SHOP_WONT_NEGO))
    price = negotiate(ch, keeper, 0, price, 0, TRUE);
  if (sell->type == SELL_AVAIL && GET_AVAIL_OFFSET(ch))
    price += bprice * GET_AVAIL_OFFSET(ch);

  // Attempt to order the item.
  if (sell->type == SELL_AVAIL && GET_OBJ_AVAILTN(obj) > 0)
  {
    // Don't let people re-try repeatedly.
    for (int q = 0; q < SHOP_LAST_IDNUM_LIST_SIZE; q++) {
      if (sell->lastidnum[q] == GET_IDNUM(ch)) {
        snprintf(buf, sizeof(buf), "%s Sorry, I couldn't get that in for you. Try again tomorrow.", GET_CHAR_NAME(ch));
        do_say(keeper, buf, cmd_say, SCMD_SAYTO);
        extract_obj(obj);
        return;
      }
    }

    // Stop people from buying enormous quantities.
    extern int max_things_you_can_purchase_at_once;
    if (buynum > max_things_you_can_purchase_at_once) {
      snprintf(buf, sizeof(buf), "%s I can't get that many in at once. Limit is %d.", GET_CHAR_NAME(ch), max_things_you_can_purchase_at_once);
      do_say(keeper, buf, cmd_say, SCMD_SAYTO);
      extract_obj(obj);
      return;
    }

    // Prevent trying to pre-order something if you don't have the scratch. Calculated using the flat price, not the negotiated one.
    int preorder_cost_for_one_object = GET_OBJ_COST(obj) / PREORDER_COST_DIVISOR;

    if (!cred || shop_table[shop_nr].type == SHOP_BLACK) {
      cash = TRUE;
      cred = NULL;
    }

    if (!cash && !cred) {
      mudlog("SYSERR: Ended up with !cash and !cred in shop purchasing!", ch, LOG_SYSLOG, TRUE);
      send_to_char(ch, "Sorry, something went wrong.\r\n");
      extract_obj(obj);
      return;
    }

    if ((cash && GET_NUYEN(ch) < preorder_cost_for_one_object * buynum)
        || (cred && GET_ITEM_MONEY_VALUE(cred) < preorder_cost_for_one_object * buynum))
    {
      snprintf(buf, sizeof(buf), "%s It'll cost you %d nuyen to place that order. Come back when you've got the funds.", GET_CHAR_NAME(ch), preorder_cost_for_one_object * buynum);
      do_say(keeper, buf, cmd_say, SCMD_SAYTO);
      extract_obj(obj);
      return;
    }

    // Calculate TNs, factoring in settings, powers, and racism.
    int target = GET_OBJ_AVAILTN(obj) - GET_AVAIL_OFFSET(ch);
    target = MAX(0, target - GET_POWER(ch, ADEPT_KINESICS));
    if (GET_RACE(ch) != GET_RACE(keeper)) {
      switch (GET_RACE(ch)) {
        case RACE_HUMAN:
        case RACE_ELF:
        case RACE_ORK:
        case RACE_TROLL:
        case RACE_DWARF:
          break;
        default:
          target += 4;
          break;
      }
    }

    // Calculate their skill level, including bioware.
    bool pheremones = FALSE;
    int skill = get_skill(ch, shop_table[shop_nr].ettiquete, target);
    for (struct obj_data *bio = ch->bioware; bio; bio = bio->next_content)
      if (GET_OBJ_VAL(bio, 0) == BIO_TAILOREDPHEREMONES) {
        pheremones = TRUE;
        skill += GET_OBJ_VAL(bio, 2) ? GET_OBJ_VAL(bio, 1) * 2: GET_OBJ_VAL(bio, 1);
        break;
      }

    // Roll up the success test.
    int success = success_test(skill, target);

    // Failure case.
    if (success < 1) {
      if (GET_SKILL(ch, shop_table[shop_nr].ettiquete) == 0) {
        if (pheremones)
          snprintf(buf, sizeof(buf), "Not even your tailored pheremones can soothe $N's annoyance at your lack of %s.\r\n",
                   skills[shop_table[shop_nr].ettiquete].name);
        else
          snprintf(buf, sizeof(buf), "$N seems annoyed that you don't even know the basics of %s.\r\n",
                   skills[shop_table[shop_nr].ettiquete].name);
      } else {
        snprintf(buf, sizeof(buf), "You exert every bit of %s you can muster, %sbut $N shakes $S head after calling a few contacts.\r\n",
                 skills[shop_table[shop_nr].ettiquete].name,
                 pheremones ? "aided by your tailored pheremones, " : "");
      }
      act(buf, FALSE, ch, 0, keeper, TO_CHAR);

      snprintf(buf, sizeof(buf), "%s I can't get ahold of that one for a while.", GET_CHAR_NAME(ch));
      do_say(keeper, buf, cmd_say, SCMD_SAYTO);

      // Add them to the forbidden list.
      for (int q = SHOP_LAST_IDNUM_LIST_SIZE - 1; q >= 1; q--)
        sell->lastidnum[q] = sell->lastidnum[q-1];
      sell->lastidnum[0] = GET_IDNUM(ch);

      extract_obj(obj);
      return;
    }

    if (GET_SKILL(ch, shop_table[shop_nr].ettiquete) == 0) {
      snprintf(buf, sizeof(buf), "$N seems annoyed that you don't even know the basics of %s, but %syou convince $M to call a few contacts anyways.\r\n",
              skills[shop_table[shop_nr].ettiquete].name,
              pheremones ? "aided by your tailored pheremones, " : "");
    } else {
      snprintf(buf, sizeof(buf), "You exert every bit of %s you can muster, %sand $N nods to you after calling a few contacts.\r\n",
               skills[shop_table[shop_nr].ettiquete].name,
               pheremones ? "aided by your tailored pheremones, " : "");
    }
    act(buf, FALSE, ch, 0, keeper, TO_CHAR);

    // Placed order successfully. Order time is multiplied by 10% per availoffset tick, then multiplied again by quantity.
    float totaltime = (GET_OBJ_AVAILDAY(obj) * (GET_AVAIL_OFFSET(ch) ? 0.1 * GET_AVAIL_OFFSET(ch) : 1) * buynum) / success;

    if (access_level(ch, LVL_ADMIN)) {
      send_to_char(ch, "You use your staff powers to greatly accelerate the ordering process (was %.2f days).\r\n", totaltime);
      totaltime = 0.0;
    }

    // Pay the preorder cost.
    if (cash) {
      lose_nuyen(ch, preorder_cost_for_one_object * buynum, NUYEN_OUTFLOW_SHOP_PURCHASES);
    } else {
      lose_nuyen_from_credstick(ch, cred, preorder_cost_for_one_object * buynum, NUYEN_OUTFLOW_SHOP_PURCHASES);
    }
    send_to_char(ch, "You put down a %d nuyen deposit on your order.\r\n", preorder_cost_for_one_object * buynum);

    if (totaltime < 1) {
      int hours = MAX(1, (int)(24 * totaltime));
      snprintf(buf, sizeof(buf), "%s That will take about %d hour%s to come in.", GET_CHAR_NAME(ch), hours, hours == 1 ? "" : "s");
    } else {
      snprintf(buf, sizeof(buf), "%s That will take about %d day%s to come in.", GET_CHAR_NAME(ch), (int) totaltime, totaltime == 1 ? "" : "s");
    }
    do_say(keeper, buf, cmd_say, SCMD_SAYTO);

    // If they have a pre-existing order, just bump up the quantity and update the order time.
    struct shop_order_data *order = shop_table[shop_nr].order;
    for (; order; order = order->next)
      if (order->player == GET_IDNUM(ch) && order->item == sell->vnum)
        break;

    if (order) {
      if (order->timeavail < time(0))
        order->timeavail = time(0);
      order->number += buynum;
      order->timeavail = order->timeavail + (int)(SECS_PER_MUD_DAY * totaltime);
    } else {
      // Create a new order.
      struct shop_order_data *order = new shop_order_data;
      order->item = sell->vnum;
      order->player = GET_IDNUM(ch);
      order->timeavail = time(0) + (int)(SECS_PER_MUD_DAY * totaltime);
      order->number = buynum;
      order->price = price;
      order->next = shop_table[shop_nr].order;
      order->sent = FALSE;
      shop_table[shop_nr].order = order;
      order->paid = preorder_cost_for_one_object;
      order->expiration = order->timeavail + (60 * 60 * 24 * PREORDERS_ARE_GOOD_FOR_X_DAYS);
    }

    // Clean up.
    extract_obj(obj);
    obj = NULL;
  } else
  {
    // Give them the thing without fanfare.
    shop_receive(ch, keeper, arg, buynum, cash, sell, obj, shop_table[shop_nr].type == SHOP_BLACK ? NULL : cred, price, shop_nr, NULL);
  }
}

void shop_sell(char *arg, struct char_data *ch, struct char_data *keeper, vnum_t shop_nr)
{
  char buf[MAX_STRING_LENGTH], buf3[MAX_STRING_LENGTH];

  if (!is_open(keeper, shop_nr))
    return;
  if (!is_ok_char(keeper, ch, shop_nr))
    return;
  struct obj_data *obj = NULL, *cred = get_first_credstick(ch, "credstick");
  struct shop_sell_data *sell = shop_table[shop_nr].selling;
  if (!*arg)
  {
    send_to_char("What item do you want to sell?\r\n", ch);
    return;
  }
  strcpy(buf, GET_CHAR_NAME(ch));
  if (shop_table[shop_nr].flags.IsSet(SHOP_DOCTOR))
  {
    if (!(obj = get_obj_in_list_vis(ch, arg, ch->carrying))
        && !(obj = get_obj_in_list_vis(ch, arg, ch->cyberware))
        && !(obj = get_obj_in_list_vis(ch, arg, ch->bioware))) {
      snprintf(ENDOF(buf), sizeof(buf) - strlen(buf), " %s", shop_table[shop_nr].no_such_itemp);
      do_say(keeper, buf, cmd_say, SCMD_SAYTO);
      return;
    }
  } else
  {
    if (!(obj = get_obj_in_list_vis(ch, arg, ch->carrying))) {
      snprintf(ENDOF(buf), sizeof(buf) - strlen(buf), " %s", shop_table[shop_nr].no_such_itemp);
      do_say(keeper, buf, cmd_say, SCMD_SAYTO);
      return;
    }
  }

  if (GET_OBJ_TYPE(obj) == ITEM_SHOPCONTAINER) {
    if (!shop_table[shop_nr].flags.IsSet(SHOP_DOCTOR)) {
      snprintf(ENDOF(buf), sizeof(buf) - strlen(buf), " I won't buy %s off of you. Take it to a cyberdoc.", GET_OBJ_NAME(obj));
      do_say(keeper, buf, cmd_say, SCMD_SAYTO);
      return;
    }

    if (!obj->contains) {
      send_to_char(ch, "%s is empty!\r\n", capitalize(GET_OBJ_NAME(obj)));
      snprintf(buf, sizeof(buf), "SYSERR: Shop container '%s' is empty!", GET_OBJ_NAME(obj));
      mudlog(buf, ch, LOG_SYSLOG, TRUE);
      return;
    }

    obj = obj->contains;
  }

  if (!shop_table[shop_nr].buytypes.IsSet(GET_OBJ_TYPE(obj)) || IS_OBJ_STAT(obj, ITEM_NOSELL) || GET_OBJ_COST(obj) < 1)
  {
    snprintf(ENDOF(buf), sizeof(buf) - strlen(buf), " %s", shop_table[shop_nr].doesnt_buy);
    do_say(keeper, buf, cmd_say, SCMD_SAYTO);
    return;
  }
  if (shop_table[shop_nr].type == SHOP_LEGAL && !cred)
  {
    snprintf(ENDOF(buf), sizeof(buf) - strlen(buf), " No cred, no business.");
    do_say(keeper, buf, cmd_say, SCMD_SAYTO);
    return;
  }
  int sellprice = sell_price(obj, shop_nr);
  if (!shop_table[shop_nr].flags.IsSet(SHOP_WONT_NEGO))
    sellprice = negotiate(ch, keeper, 0, sellprice, 0, 0);

  if (shop_table[shop_nr].flags.IsSet(SHOP_DOCTOR) && !obj->in_obj)
    uninstall_ware_from_target_character(obj, keeper, ch, !shop_table[shop_nr].flags.IsSet(SHOP_CHARGEN));
  else {
    if (obj->in_obj) {
      struct obj_data *container = obj->in_obj;

      // Pull the 'ware out.
      obj_from_obj(obj);

      // Remove the container and junk it.
      obj_from_char(container);
      extract_obj(container);
      container = NULL;
    } else {
      obj_from_char(obj);
    }
  }

  if (!cred || shop_table[shop_nr].type == SHOP_BLACK)
    gain_nuyen(ch, sellprice, NUYEN_INCOME_SHOP_SALES);
  else
    gain_nuyen_on_credstick(ch, cred, sellprice, NUYEN_INCOME_SHOP_SALES);
  snprintf(buf3, sizeof(buf3), "%s sold %s^g at %s^g (%ld) for %d.", GET_CHAR_NAME(ch), GET_OBJ_NAME(obj), GET_CHAR_NAME(keeper), shop_table[shop_nr].vnum, sellprice);
  mudlog(buf3, ch, LOG_GRIDLOG, TRUE);

  // Write the nuyen cost to buf3 and the current buy-string to arg.
  char price_buf[100];
  snprintf(price_buf, sizeof(price_buf), "%d", sellprice);
  strcpy(arg, shop_table[shop_nr].sell);

  // Use our new replace_substring() function to swap out all %d's in arg with the nuyen string.
  replace_substring(arg, buf3, "%d", price_buf);

  // Compose the sayto string for the keeper.
  snprintf(buf, sizeof(buf), "%s %s", GET_CHAR_NAME(ch), buf3);
  do_say(keeper, buf, cmd_say, SCMD_SAYTO);
  for (;sell; sell = sell->next)
    if (sell->vnum == GET_OBJ_VNUM(obj))
      break;
  if (!sell)
  {
    if (!shop_table[shop_nr].flags.IsSet(SHOP_NORESELL) && GET_OBJ_TYPE(obj) != ITEM_GUN_MAGAZINE && GET_OBJ_TYPE(obj) !=
        ITEM_CYBERWARE && GET_OBJ_TYPE(obj) != ITEM_BIOWARE) {
      sell = new shop_sell_data;
      sell->type = SELL_BOUGHT;
      sell->stock = 1;
      sell->vnum = GET_OBJ_VNUM(obj);
      sell->next = shop_table[shop_nr].selling;
      shop_table[shop_nr].selling = sell;
    }
    int x = 0;
    for (sell = shop_table[shop_nr].selling; sell; sell = sell->next)
      x++;
    if (x > 50) {
      struct shop_sell_data *temp;
      x = number(1, x-1);
      for (sell = shop_table[shop_nr].selling; sell && x > 0; sell = sell->next)
        x--;
      REMOVE_FROM_LIST(sell, shop_table[shop_nr].selling, next);
    }


  } else if (sell->type == SELL_STOCK || sell->type == SELL_BOUGHT)
    sell->stock++;

 extract_obj(obj);
 obj = NULL;
}

void shop_list(char *arg, struct char_data *ch, struct char_data *keeper, vnum_t shop_nr)
{
  char buf[MAX_STRING_LENGTH];
  if (!is_open(keeper, shop_nr))
    return;

  /*   We allow anyone to view the list, on the presumption that you're just browsing the shelves or whatnot.
  if (!is_ok_char(keeper, ch, shop_nr))
    return;
  */

  struct obj_data *obj;
  int i = 1;

  if (PRF_FLAGGED(ch, PRF_SCREENREADER)) {
    snprintf(buf, sizeof(buf), "%s has the following items available for sale:\r\n", GET_NAME(keeper));

    for (struct shop_sell_data *sell = shop_table[shop_nr].selling; sell; sell = sell->next, i++) {
      // Read the object; however, if it's an invalid vnum or has no sale cost, skip it.
      obj = read_object(sell->vnum, VIRTUAL);
      if (!can_sell_object(obj, keeper, shop_nr)) {
        i--;
        continue;
      }

      // List the item to the player.
      snprintf(ENDOF(buf), sizeof(buf) - strlen(buf), "Item #%d: %s for %d nuyen", i, GET_OBJ_NAME(obj), buy_price(obj, shop_nr));

      // Doctorshop? Tack on bioware / cyberware info.
      if (shop_table[shop_nr].flags.IsSet(SHOP_DOCTOR)) {
        snprintf(ENDOF(buf), sizeof(buf) - strlen(buf), "; it's %s that costs %.2f %s",
                GET_OBJ_TYPE(obj) == ITEM_CYBERWARE ? "cyberware" : "bioware",
                ((float)GET_OBJ_VAL(obj, 4) / 100),
                GET_OBJ_TYPE(obj) == ITEM_CYBERWARE ? "essence" : "bio index");

      }

      // Finish up with availability info.
      if (!(sell->type == SELL_ALWAYS) && !(sell->type == SELL_AVAIL && GET_OBJ_AVAILDAY(obj) == 0)) {
        if (sell->type == SELL_AVAIL) {
          int arbitrary_difficulty = GET_OBJ_AVAILTN(obj);
          if (arbitrary_difficulty <= 2) {
            strlcat(buf, ". It's a trivial special order", sizeof(buf));
          } else if (arbitrary_difficulty <= 4) {
            strlcat(buf, ". It's an easy special order", sizeof(buf));
          } else if (arbitrary_difficulty <= 7) {
            strlcat(buf, ". It's a moderately-difficult special order", sizeof(buf));
          } else if (arbitrary_difficulty <= 10) {
            strlcat(buf, ". It's a special order that will take some serious convincing", sizeof(buf));
          } else {
            strlcat(buf, ". It's a special order that probably needs a fixer", sizeof(buf));
          }
        } else if (sell->stock <= 0) {
          strlcat(buf, ". It is currently out of stock", sizeof(buf));
        } else {
          snprintf(ENDOF(buf), sizeof(buf) - strlen(buf), ". Only %d %s in stock", sell->stock, sell->stock > 1 ? "are" : "is");
        }
      }

      if (IS_OBJ_STAT(obj, ITEM_NERPS))
        strlcat(buf, ". OOC note: It has no coded effect", sizeof(buf));

      strlcat(buf, ".\r\n", sizeof(buf));

      // Clean up so we don't leak the object.
      extract_obj(obj);
      obj = NULL;
    }
    strlcat(buf, "\r\nYou can use the PROBE or INFO commands for more details.\r\n", sizeof(buf));
    page_string(ch->desc, buf, 1);
    return;
  }


  if (shop_table[shop_nr].flags.IsSet(SHOP_DOCTOR)) {
    strlcpy(buf, " **   Availability     Item                              Rating  Ess/Index     Price\r\n"
                 "------------------------------------------------------------------------------------\r\n", sizeof(buf));

    for (struct shop_sell_data *sell = shop_table[shop_nr].selling; sell; sell = sell->next, i++) {
      obj = read_object(sell->vnum, VIRTUAL);
      if (!can_sell_object(obj, keeper, shop_nr)) {
        i--;
        continue;
      }
      snprintf(ENDOF(buf), sizeof(buf) - strlen(buf), " %2d)  ", i);
      if (sell->type == SELL_ALWAYS || (sell->type == SELL_AVAIL && GET_OBJ_AVAILDAY(obj) == 0))
        snprintf(ENDOF(buf), sizeof(buf) - strlen(buf), "Yes            ");
      else if (sell->type == SELL_AVAIL) {
        int arbitrary_difficulty = GET_OBJ_AVAILTN(obj);
        if (arbitrary_difficulty <= 2) {
          strlcat(buf, "Sp. Ord (triv)   ", sizeof(buf));
        } else if (arbitrary_difficulty <= 4) {
          strlcat(buf, "Sp. Ord (easy)   ", sizeof(buf));
        } else if (arbitrary_difficulty <= 7) {
          strlcat(buf, "Sp. Ord (med)    ", sizeof(buf));
        } else if (arbitrary_difficulty <= 10) {
          strlcat(buf, "Sp. Ord (hard)   ", sizeof(buf));
        } else {
          strlcat(buf, "Sp. Ord (fixer)  ", sizeof(buf));
        }
        /*
        if (GET_OBJ_AVAILDAY(obj) < 1)
          snprintf(ENDOF(buf), sizeof(buf) - strlen(buf), "~%-2d Hours      ", (int)(24 * GET_OBJ_AVAILDAY(obj)));
        else
          snprintf(ENDOF(buf), sizeof(buf) - strlen(buf), "~%-2d Day%c       ", (int)GET_OBJ_AVAILDAY(obj), GET_OBJ_AVAILDAY(obj) > 1 ? 's' : ' ');
        */
      } else {
        if (sell->stock <= 0)
          snprintf(ENDOF(buf), sizeof(buf) - strlen(buf), "(Out Of Stock) ");
        else
          snprintf(ENDOF(buf), sizeof(buf) - strlen(buf), "%-3d            ", sell->stock);
      }
      if (GET_OBJ_VAL(obj, 1) > 0)
        snprintf(buf2, sizeof(buf2), "%d", GET_OBJ_VAL(obj, 1));
      else strcpy(buf2, "-");

      if (IS_OBJ_STAT(obj, ITEM_NERPS)) {
        snprintf(ENDOF(buf), sizeof(buf) - strlen(buf), "^Y(N)^n %-29s^n %-6s%2s   %0.2f%c  %9d\r\n", GET_OBJ_NAME(obj),
                GET_OBJ_TYPE(obj) == ITEM_CYBERWARE ? "Cyber" : "Bio", buf2, ((float)GET_OBJ_VAL(obj, 4) / 100),
                GET_OBJ_TYPE(obj) == ITEM_CYBERWARE ? 'E' : 'I', buy_price(obj, shop_nr));
      } else {
        snprintf(ENDOF(buf), sizeof(buf) - strlen(buf), "%-33s^n %-6s%2s   %0.2f%c  %9d\r\n", GET_OBJ_NAME(obj),
                GET_OBJ_TYPE(obj) == ITEM_CYBERWARE ? "Cyber" : "Bio", buf2, ((float)GET_OBJ_VAL(obj, 4) / 100),
                GET_OBJ_TYPE(obj) == ITEM_CYBERWARE ? 'E' : 'I', buy_price(obj, shop_nr));
      }
      extract_obj(obj);
      obj = NULL;
    }
  } else
  {
    strcpy(buf, " **   Availability     Item                                              Price\r\n"
                "--------------------------------------------------------------------------------\r\n");
    for (struct shop_sell_data *sell = shop_table[shop_nr].selling; sell; sell = sell->next, i++) {
      obj = read_object(sell->vnum, VIRTUAL);
      if (!can_sell_object(obj, keeper, shop_nr)) {
        i--;
        continue;
      }
      snprintf(ENDOF(buf), sizeof(buf) - strlen(buf), " %2d)  ", i);
      if (sell->type == SELL_ALWAYS || (sell->type == SELL_AVAIL && GET_OBJ_AVAILDAY(obj) == 0))
        snprintf(ENDOF(buf), sizeof(buf) - strlen(buf), "Yes            ");
      else if (sell->type == SELL_AVAIL) {
        int arbitrary_difficulty = GET_OBJ_AVAILTN(obj);
        if (arbitrary_difficulty < 3) {
          strlcat(buf, "Sp. Ord (triv)   ", sizeof(buf));
        } else if (arbitrary_difficulty < 5) {
          strlcat(buf, "Sp. Ord (easy)   ", sizeof(buf));
        } else if (arbitrary_difficulty < 8) {
          strlcat(buf, "Sp. Ord (med)    ", sizeof(buf));
        } else if (arbitrary_difficulty < 10) {
          strlcat(buf, "Sp. Ord (hard)   ", sizeof(buf));
        } else if (arbitrary_difficulty < 14) {
          strlcat(buf, "Sp. Ord (hard+)  ", sizeof(buf));
        } else {
          strlcat(buf, "Sp. Ord (fixer)  ", sizeof(buf));
        }
      } else {
        if (sell->stock <= 0)
          snprintf(ENDOF(buf), sizeof(buf) - strlen(buf), "(Out Of Stock) ");
        else
          snprintf(ENDOF(buf), sizeof(buf) - strlen(buf), "%-3d            ", sell->stock);
      }

      if (IS_OBJ_STAT(obj, ITEM_NERPS)) {
        snprintf(ENDOF(buf), sizeof(buf) - strlen(buf), "^Y(N)^n %-44s^n %6d\r\n", GET_OBJ_NAME(obj), buy_price(obj, shop_nr));
      } else {
        snprintf(ENDOF(buf), sizeof(buf) - strlen(buf), "%-48s^n %6d\r\n", GET_OBJ_NAME(obj),
                  buy_price(obj, shop_nr));
      }
      if (strlen(buf) >= MAX_STRING_LENGTH - 200) {
        snprintf(buf2, sizeof(buf2), "Shop %ld ('%s'): Aborting string composition due to length constraints.", shop_table[shop_nr].vnum, GET_NAME(keeper));
        break;
      }
      extract_obj(obj);
      obj = NULL;
    }
  }

  // New characters get reminded about the probe and info commands.
  if (GET_TKE(ch) <= 2 * NEWBIE_KARMA_THRESHOLD)
    strlcat(buf, "\r\nUse ^WPROBE^n for more details.\r\n", sizeof(buf));
  page_string(ch->desc, buf, 1);
}

void shop_value(char *arg, struct char_data *ch, struct char_data *keeper, vnum_t shop_nr)
{
  char buf[MAX_STRING_LENGTH];
  if (!is_open(keeper, shop_nr))
    return;
  if (!is_ok_char(keeper, ch, shop_nr))
    return;
  struct obj_data *obj;
  strcpy(buf, GET_CHAR_NAME(ch));
  if (!*arg)
  {
    send_to_char("What item do you want valued?\r\n", ch);
    return;
  }
  if (shop_table[shop_nr].flags.IsSet(SHOP_DOCTOR))
  {
    if (!(obj = get_obj_in_list_vis(ch, arg, ch->cyberware))
        && !(obj = get_obj_in_list_vis(ch, arg, ch->bioware))
        && !(obj = get_obj_in_list_vis(ch, arg, ch->carrying))) {
      send_to_char(ch, "You don't seem to have a '%s'.\r\n", arg);
      return;
    }
  } else
  {
    if (!(obj = get_obj_in_list_vis(ch, arg, ch->carrying))) {
      send_to_char(ch, "You don't seem to have a '%s'.\r\n", arg);
      return;
    }
  }

  if (GET_OBJ_TYPE(obj) == ITEM_SHOPCONTAINER) {
    if (!shop_table[shop_nr].flags.IsSet(SHOP_DOCTOR)) {
      snprintf(buf, sizeof(buf), "%s I wouldn't buy %s off of you. Take it to a cyberdoc.", GET_CHAR_NAME(ch), GET_OBJ_NAME(obj));
      do_say(keeper, buf, cmd_say, SCMD_SAYTO);
      return;
    }

    if (!obj->contains) {
      send_to_char(ch, "%s is empty!\r\n", capitalize(GET_OBJ_NAME(obj)));
      snprintf(buf, sizeof(buf), "SYSERR: Shop container '%s' is empty!", GET_OBJ_NAME(obj));
      mudlog(buf, ch, LOG_SYSLOG, TRUE);
      return;
    }

    obj = obj->contains;

    int install_cost = MIN(CYBERWARE_INSTALLATION_COST_MAXIMUM, GET_OBJ_COST(obj) / CYBERWARE_INSTALLATION_COST_FACTOR);

    snprintf(buf, sizeof(buf), "%s I'd charge %d nuyen to install it, and", GET_CHAR_NAME(ch), install_cost);
  } else {
    // Since we're not pre-filling buf with something else to say, just stick the name in for the sayto target.
    strcpy(buf, GET_CHAR_NAME(ch));
  }

  if (!shop_table[shop_nr].buytypes.IsSet(GET_OBJ_TYPE(obj)) || IS_OBJ_STAT(obj, ITEM_NOSELL))
    snprintf(ENDOF(buf), sizeof(buf) - strlen(buf), " I wouldn't buy %s off of you.", GET_OBJ_NAME(obj));
  else
    snprintf(ENDOF(buf), sizeof(buf) - strlen(buf), " I would be able to give you around %d nuyen for %s.", sell_price(obj, shop_nr), GET_OBJ_NAME(obj));

  do_say(keeper, buf, cmd_say, SCMD_SAYTO);
}

bool shop_probe(char *arg, struct char_data *ch, struct char_data *keeper, vnum_t shop_nr) {
  if (!is_open(keeper, shop_nr))
    return FALSE;
  // if (!is_ok_char(keeper, ch, shop_nr))
  //  return FALSE;

  struct obj_data *obj = NULL;
  skip_spaces(&arg);

  if (!*arg) {
    // No error message, let do_probe() handle it.
    return FALSE;
  }

  struct shop_sell_data *sell = find_obj_shop(arg, shop_nr, &obj);
  if (!sell && atoi(arg) > 0) {
    // Adapt for the player probably meaning an item number instead of an item with a numeric keyword.
    char oopsbuf[strlen(arg) + 2];
    snprintf(oopsbuf, sizeof(oopsbuf), "#%s", arg);
    sell = find_obj_shop(oopsbuf, shop_nr, &obj);
  }

  if (!sell || !obj) {
    return FALSE;
  }

  send_to_char(ch, "^yProbing shopkeeper's ^n%s^y...^n\r\n", GET_OBJ_NAME(obj));
  do_probe_object(ch, obj);
  return TRUE;

  return FALSE;
}

void shop_info(char *arg, struct char_data *ch, struct char_data *keeper, vnum_t shop_nr)
{
  char buf[MAX_STRING_LENGTH];
  int num = 0, num2 = 0;
  if (!is_open(keeper, shop_nr))
    return;
  if (!is_ok_char(keeper, ch, shop_nr))
    return;
  struct obj_data *obj;
  skip_spaces(&arg);

  if (!*arg) {
    send_to_char("Syntax: INFO <item>\r\n", ch);
    return;
  }

  if (!find_obj_shop(arg, shop_nr, &obj))
  {
    bool successful = FALSE;
    if (atoi(arg) > 0) {
      // Adapt for the player probably meaning an item number instead of an item with a numeric keyword.
      char oopsbuf[strlen(arg) + 2];
      snprintf(oopsbuf, sizeof(oopsbuf), "#%s", arg);
      successful = (find_obj_shop(oopsbuf, shop_nr, &obj) != NULL);
    }
    if (!successful) {
      snprintf(buf, sizeof(buf), "%s I don't have that item.", GET_CHAR_NAME(ch));
      do_say(keeper, buf, cmd_say, SCMD_SAYTO);
      return;
    }
  }
  snprintf(buf, sizeof(buf), "%s %s is", GET_CHAR_NAME(ch), CAP(obj->text.name));
  switch (GET_OBJ_TYPE(obj))
  {
  case ITEM_WEAPON:
    if (IS_GUN(GET_OBJ_VAL(obj, 3))) {
      if (GET_OBJ_VAL(obj, 0) < 3)
        strcat(buf, " a weak");
      else if (GET_OBJ_VAL(obj, 0) < 6)
        strcat(buf, " a low powered");
      else if (GET_OBJ_VAL(obj, 0) < 10)
        strcat(buf, " a moderately powered");
      else if (GET_OBJ_VAL(obj, 0) < 12)
        strcat(buf, " a strong");
      else
        strcat(buf, " a high powered");

      if (IS_SET(GET_OBJ_VAL(obj, 10), 1 << MODE_SS))
        strcat(buf, " single shot");
      else if (IS_SET(GET_OBJ_VAL(obj, 10), 1 << MODE_FA))
        strcat(buf, " fully automatic");
      else if (IS_SET(GET_OBJ_VAL(obj, 10), 1 << MODE_BF))
        strcat(buf, " burst-fire");
      else if (IS_SET(GET_OBJ_VAL(obj, 10), 1 << MODE_SA))
        strcat(buf, " semi-automatic");
      snprintf(ENDOF(buf), sizeof(buf) - strlen(buf), " %s", weapon_type[GET_OBJ_VAL(obj, 3)]);
      if (IS_OBJ_STAT(obj, ITEM_TWOHANDS))
        strcat(buf, " and requires two hands to wield correctly");
      if (GET_WEAPON_INTEGRAL_RECOIL_COMP(obj))
        snprintf(ENDOF(buf), sizeof(buf) - strlen(buf), ". It has %d round%s of built-in recoil compensation",
                GET_WEAPON_INTEGRAL_RECOIL_COMP(obj),
                GET_WEAPON_INTEGRAL_RECOIL_COMP(obj) > 1 ? "s" : "");
      if (GET_OBJ_VAL(obj, 7) > 0 || GET_OBJ_VAL(obj, 8) > 0 || GET_OBJ_VAL(obj, 9) > 0)
        strcat(buf, ". It comes standard with ");

      int real_obj;
      if (GET_OBJ_VAL(obj, 7) > 0 && (real_obj = real_object(GET_OBJ_VAL(obj, 7))) > 0) {
        strcat(buf, obj_proto[real_obj].text.name);
        if (GET_OBJ_VAL(obj, 8) > 0 && GET_OBJ_VAL(obj, 9) > 0)
          strcat(buf, ", ");
        else if ((GET_OBJ_VAL(obj, 8) > 0 || GET_OBJ_VAL(obj, 9) > 0))
          strcat(buf, " and ");

      }
      if (GET_OBJ_VAL(obj, 8) > 0 && (real_obj = real_object(GET_OBJ_VAL(obj, 8))) > 0) {
        strcat(buf, obj_proto[real_obj].text.name);
        if (GET_OBJ_VAL(obj, 9) > 0)
          strcat(buf, " and ");
      }
      if (GET_OBJ_VAL(obj, 9) > 0 && (real_obj = real_object(GET_OBJ_VAL(obj, 9))) > 9) {
        strcat(buf, obj_proto[real_obj].text.name);
      }
      snprintf(ENDOF(buf), sizeof(buf) - strlen(buf), ". It can hold a maximum of %d rounds.", GET_OBJ_VAL(obj, 5));
    } else {
      // Map damage value to phrase.
      if (GET_WEAPON_DAMAGE_CODE(obj) == LIGHT) {
        strcat(buf, " a lightly-damaging");
      } else if (GET_WEAPON_DAMAGE_CODE(obj) == MODERATE) {
        strcat(buf, " a moderately-damaging");
      } else if (GET_WEAPON_DAMAGE_CODE(obj) == SERIOUS) {
        strcat(buf, " a strong");
      } else if (GET_WEAPON_DAMAGE_CODE(obj) == DEADLY) {
        strcat(buf, " a deadly");
      } else {
        strcat(buf, " an indeterminate-strength");
        snprintf(buf1, sizeof(buf1), "SYSERR: Unable to map damage value %d for weapon '%s' (%ld) to a damage phrase.",
                GET_WEAPON_DAMAGE_CODE(obj), GET_OBJ_NAME(obj), GET_OBJ_VNUM(obj));
        mudlog(buf1, NULL, LOG_SYSLOG, TRUE);
      }
      snprintf(ENDOF(buf), sizeof(buf) - strlen(buf), " %s", weapon_type[GET_OBJ_VAL(obj, 3)]);

      // Two-handed weapon?
      if (IS_OBJ_STAT(obj, ITEM_TWOHANDS))
        strcat(buf, " that requires two hands to wield correctly.");
      else
        strcat(buf, ".");

      // Reach?
      if (GET_WEAPON_REACH(obj)) {
        snprintf(ENDOF(buf), sizeof(buf) - strlen(buf), " As a long weapon, it gives you %d meter%s of extended reach.",
                GET_WEAPON_REACH(obj), GET_WEAPON_REACH(obj) > 1 ? "s" : "");
      }

      if (GET_WEAPON_FOCUS_RATING(obj) > 0) {
        snprintf(ENDOF(buf), sizeof(buf) - strlen(buf), " It is a weapon focus of force %d.", GET_WEAPON_FOCUS_RATING(obj));
      }

      // Map strength bonus to phrase.
      if (GET_WEAPON_STR_BONUS(obj) != 0) {
        if (GET_WEAPON_STR_BONUS(obj) == 1) {
          strcat(buf, " It is somewhat well-constructed and will let you hit a little harder in combat.");
        } else if (GET_WEAPON_STR_BONUS(obj) == 2) {
          strcat(buf, " It is well-constructed, letting you land strong hits.");
        } else if (GET_WEAPON_STR_BONUS(obj) == 3) {
          strcat(buf, " It is extremely well-constructed, letting you hit with great strength.");
        } else if (GET_WEAPON_STR_BONUS(obj) == 4) {
          strcat(buf, " It is masterfully constructed, letting you hit as hard as possible.");
        } else {
          strcat(buf, " It has an indeterminate strength modifier.");
          snprintf(buf1, sizeof(buf1), "SYSERR: Unable to map strength modifier %d for weapon '%s' (%ld) to a feature phrase.",
                  GET_WEAPON_STR_BONUS(obj), GET_OBJ_NAME(obj), GET_OBJ_VNUM(obj));
          mudlog(buf1, NULL, LOG_SYSLOG, TRUE);
        }
      }
    }
    break;
  case ITEM_WORN:
    num = (GET_OBJ_VAL(obj, 5) > 20 ? (int)(GET_OBJ_VAL(obj, 5) / 100) : GET_OBJ_VAL(obj, 5)) +
          (GET_OBJ_VAL(obj, 6) > 20 ? (int)(GET_OBJ_VAL(obj, 6) / 100) : GET_OBJ_VAL(obj, 6));
    if (num < 1)
      strcat(buf, " a piece of clothing");
    else if (num < 4)
      strcat(buf, " a lightly armored piece of clothing");
    else if (num < 7)
      strcat(buf, " a piece of light armor");
    else if (num < 10)
      strcat(buf, " a moderately rated piece of armor");
    else
      strcat(buf, " a piece of heavy armor");
    if (GET_OBJ_VAL(obj, 1) > 5)
      strcat(buf, " designed for carrying ammunition");
    else if (GET_OBJ_VAL(obj, 4) > 3 && GET_OBJ_VAL(obj, 4) < 6)
      strcat(buf, " that can carry a bit of gear");
    else if (GET_OBJ_VAL(obj, 4) >= 6)
      strcat(buf, " that can carry a lot of gear");
    if (GET_OBJ_VAL(obj, 7) < -2)
      strcat(buf, ". It is also very bulky.");
    else if (GET_OBJ_VAL(obj, 7) < 1)
      strcat(buf, ". It is easier to see under clothing.");
    else if (GET_OBJ_VAL(obj, 7) < 4)
      strcat(buf, ". It is quite concealable.");
    else
      strcat(buf, ". It's almost invisible under clothing.");
    break;
  case ITEM_PROGRAM:
    snprintf(ENDOF(buf), sizeof(buf) - strlen(buf), " a rating %d %s program, that is %dMp in size.", GET_OBJ_VAL(obj, 1),
            programs[GET_OBJ_VAL(obj, 0)].name, GET_OBJ_VAL(obj, 2));
    break;
  case ITEM_CYBERDECK:
    if (GET_OBJ_VAL(obj, 0) < 4)
      strcat(buf, " a beginners cyberdeck");
    else if (GET_OBJ_VAL(obj, 0) < 9)
      strcat(buf, " a cyberdeck of moderate ability");
    else if (GET_OBJ_VAL(obj, 0) < 12)
      strcat(buf, " a top of the range cyberdeck");
    else
      strcat(buf, " one of the best cyberdecks you'll ever see");
    if (GET_OBJ_VAL(obj, 2) + GET_OBJ_VAL(obj, 3) < 600)
      strcat(buf, " with a modest amount of memory.");
    else if (GET_OBJ_VAL(obj, 2) + GET_OBJ_VAL(obj, 3) < 1400)
      strcat(buf, " containing a satisfactory amount of memory.");
    else if (GET_OBJ_VAL(obj, 2) + GET_OBJ_VAL(obj, 3) < 3000)
      strcat(buf, " featuring a fair amount of memory.");
    else if (GET_OBJ_VAL(obj, 2) + GET_OBJ_VAL(obj, 3) < 5000)
      strcat(buf, " with oodles of memory.");
    else
      strcat(buf, " with more memory than you could shake a datajack at.");
    if (GET_OBJ_VAL(obj, 1) < 2)
      strcat(buf, " You better hope you don't run into anything nasty while using it");
    else if (GET_OBJ_VAL(obj, 1) < 5)
      strcat(buf, " It offers adequate protection from feedback");
    else if (GET_OBJ_VAL(obj, 1) < 9)
      strcat(buf, " Nothing will phase you");
    else
      strcat(buf, " It could protect you from anything");
    if (GET_OBJ_VAL(obj, 4) < 100)
      strcat(buf, " but you're out of luck if you want to transfer anything.");
    else if (GET_OBJ_VAL(obj, 4) < 200)
      strcat(buf, " and it transfers slowly, but will get the job done.");
    else if (GET_OBJ_VAL(obj, 4) < 300)
      strcat(buf, " and on the plus side it's IO is excellent.");
    else if (GET_OBJ_VAL(obj, 4) < 500)
      strcat(buf, " also the IO is second to none.");
    else
      strcat(buf, " and it can upload faster than light.");
    break;
  case ITEM_FOOD:
    if (GET_OBJ_VAL(obj, 0) < 2)
      strcat(buf, " a small");
    else if (GET_OBJ_VAL(obj, 0) < 5)
      strcat(buf, " a average");
    else if (GET_OBJ_VAL(obj, 0) < 10)
      strcat(buf, " a large");
    else
      strcat(buf, " a huge");
    strcat(buf, " portion of food.");
    break;
  case ITEM_DOCWAGON:
    strcat(buf, " a DocWagon contract, it will call them out when your vital signs drop.");
    break;
  case ITEM_CONTAINER:
    if (GET_OBJ_VAL(obj, 0) < 5)
      strcat(buf, " tiny");
    else if (GET_OBJ_VAL(obj, 0) < 15)
      strcat(buf, " small");
    else if (GET_OBJ_VAL(obj, 0) < 30)
      strcat(buf, " large");
    else if (GET_OBJ_VAL(obj, 0) < 60)
      strcat(buf, " huge");
    else
      strcat(buf, " gigantic");

    if (obj->obj_flags.wear_flags.AreAnySet(ITEM_WEAR_BACK, ITEM_WEAR_ABOUT, ENDBIT))
      strcat(buf, " backpack.");
    else
      strcat(buf, " container.");
    break;
  case ITEM_DECK_ACCESSORY:
    if (GET_OBJ_VAL(obj, 0) == TYPE_COOKER) {
      strcat(buf, " a");
      if (GET_DECK_ACCESSORY_COOKER_RATING(obj) <= 2)
        strcat(buf, " sluggish");
      else if (GET_DECK_ACCESSORY_COOKER_RATING(obj) <= 4)
        strcat(buf, " average");
      else if (GET_DECK_ACCESSORY_COOKER_RATING(obj) <= 6)
        strcat(buf, " quick");
      else if (GET_DECK_ACCESSORY_COOKER_RATING(obj) <= 8)
        strcat(buf, " speedy");
      else
        strcat(buf, " lightning fast");
      strcat(buf, " optical chip encoder.");
    } else if (GET_OBJ_VAL(obj, 0) == TYPE_COMPUTER) {
      strcat(buf, " a personal computer. It has ");
      if (GET_OBJ_VAL(obj, 1) < 150)
        strcat(buf, " a small memory capacity.");
      else if (GET_OBJ_VAL(obj, 1) < 500)
        strcat(buf, " a moderate amount of memory.");
      else if (GET_OBJ_VAL(obj, 1) < 1500)
        strcat(buf, " a large amount of memory.");
      else if (GET_OBJ_VAL(obj, 1) < 3000)
        strcat(buf, " more than enough memory for most people.");
      else
        strcat(buf, " an almost unimaginable amount of memory.");
    } else if (GET_OBJ_VAL(obj, 0) == TYPE_PARTS)
      strcat(buf, " used in the construction of cyberdeck components.");
    else if (GET_OBJ_VAL(obj, 0) == TYPE_UPGRADE && GET_OBJ_VAL(obj, 1) == 3)
      strcat(buf, " used to allow other people to surf along side you.");
    break;
  case ITEM_SPELL_FORMULA:
    snprintf(ENDOF(buf), sizeof(buf) - strlen(buf), " a rating %d spell formula, describing %s. It is designed for use by a %s mage.", GET_OBJ_VAL(obj, 0),
            spells[GET_OBJ_VAL(obj, 1)].name, GET_OBJ_VAL(obj, 2) == 1 ? "shamanic" : "hermetic");
    break;
  case ITEM_GUN_ACCESSORY:
    snprintf(ENDOF(buf), sizeof(buf) - strlen(buf), " a firearm accessory that attaches to the %s of a weapon.", (GET_OBJ_VAL(obj, 0) == 0 ? "top" :
                                                                                     (GET_OBJ_VAL(obj, 0) == 1 ? "barrel" : "bottom")));
    break;
  case ITEM_GUN_AMMO:
    snprintf(ENDOF(buf), sizeof(buf) - strlen(buf), " a box of ammunition for reloading %s magazines. It contains %d rounds of %s ammo.",
            weapon_type[GET_AMMOBOX_WEAPON(obj)],
            GET_AMMOBOX_QUANTITY(obj),
            ammo_type[GET_AMMOBOX_TYPE(obj)].name);
    break;
  case ITEM_FOCUS:
    snprintf(ENDOF(buf), sizeof(buf) - strlen(buf), " a rating %d %s focus.", GET_OBJ_VAL(obj, 1), foci_type[GET_OBJ_VAL(obj, 0)]);
    break;
  case ITEM_MAGIC_TOOL:
    if (GET_OBJ_VAL(obj, 0) == TYPE_LIBRARY_CONJURE || GET_OBJ_VAL(obj, 0) == TYPE_LIBRARY_SPELL) {
      snprintf(ENDOF(buf), sizeof(buf) - strlen(buf), "  is a rating %d ", GET_OBJ_VAL(obj, 1));
      if (GET_OBJ_VAL(obj, 0) == TYPE_LIBRARY_CONJURE)
        strcat(buf, "conjuring");
      else if (GET_OBJ_VAL(obj, 0) == TYPE_LIBRARY_SPELL)
        strcat(buf, "sorcery");
      strcat(buf, " library.");
    }
    break;
  case ITEM_MOD:
    strcat(buf, " a vehicle modification for the ");
    if (GET_OBJ_VAL(obj, 6) >= MOD_INTAKE_FRONT && GET_OBJ_VAL(obj, 6) <= MOD_INTAKE_REAR)
      strcat(buf, "intake");
    else if (GET_OBJ_VAL(obj, 6) >= MOD_BODY_FRONT && GET_OBJ_VAL(obj, 6) <= MOD_BODY_WINDOWS)
      strcat(buf, "body");
    else if (GET_OBJ_VAL(obj, 6) >= MOD_COMPUTER1 && GET_OBJ_VAL(obj, 6) <= MOD_COMPUTER3)
      strcat(buf, "computer");
    else strcat(buf, mod_name[GET_OBJ_VAL(obj, 6)]);
    strcat(buf, ". It is for ");
    for (int q = 1; q <= ENGINE_DIESEL; q++)
      if (IS_SET(GET_OBJ_VAL(obj, 5), 1 << q))
        num++;
    if (num) {
      if (IS_SET(GET_OBJ_VAL(obj, 5), 1 << ENGINE_ELECTRIC)) {
        strcat(buf, "electric");
        num2++;
        num--;
      }
      if (IS_SET(GET_OBJ_VAL(obj, 5), 1 << ENGINE_FUELCELL)) {
        if (num2) {
          if (num > 1)
            strcat(buf, ", ");
          else strcat(buf, " and ");
        }
        strcat(buf, "fuel cell");
        num2++;
        num--;
      }
      if (IS_SET(GET_OBJ_VAL(obj, 5), 1 << ENGINE_GASOLINE)) {
        if (num2) {
          if (num > 1)
            strcat(buf, ", ");
          else strcat(buf, " and ");
        }
        strcat(buf, "gasoline");
        num2++;
        num--;
      }
      if (IS_SET(GET_OBJ_VAL(obj, 5), 1 << ENGINE_METHANE)) {
        if (num2) {
          if (num > 1)
            strcat(buf, ", ");
          else strcat(buf, " and ");
        }
        strcat(buf, "methane");
        num2++;
        num--;
      }
      if (IS_SET(GET_OBJ_VAL(obj, 5), 1 << ENGINE_DIESEL)) {
        if (num2) {
          if (num > 1)
            strcat(buf, ", ");
          else strcat(buf, " and ");
        }
        strcat(buf, "diesel");
        num2++;
        num--;
      }
    } else strcat(buf, "all");
    strcat(buf, " engines.");
    break;
  default:
    snprintf(buf, sizeof(buf), "%s I don't know anything about that.", GET_CHAR_NAME(ch));
  }
  strcat(buf, " It weighs about ");
  if (GET_OBJ_WEIGHT(obj) < 1) {
    snprintf(ENDOF(buf), sizeof(buf) - strlen(buf), "%d grams", (int)(GET_OBJ_WEIGHT(obj) * 1000));
  } else snprintf(ENDOF(buf), sizeof(buf) - strlen(buf), "%.0f kilogram%s", GET_OBJ_WEIGHT(obj), (GET_OBJ_WEIGHT(obj) >= 2 ? "s" : ""));
  snprintf(ENDOF(buf), sizeof(buf) - strlen(buf), " and I couldn't let it go for less than %d nuyen.", buy_price(obj, shop_nr));

  if (IS_OBJ_STAT(obj, ITEM_NERPS)) {
    strcat(buf, " ^Y(OOC: It has no special coded effects.)^n");
  }

  do_say(keeper, buf, cmd_say, SCMD_SAYTO);
  send_to_char(ch, "\r\n%s\r\n\r\n", obj->text.look_desc);
}

void shop_check(char *arg, struct char_data *ch, struct char_data *keeper, vnum_t shop_nr)
{
  char buf[MAX_STRING_LENGTH];
  if (!is_open(keeper, shop_nr))
    return;
  if (!is_ok_char(keeper, ch, shop_nr))
    return;
  int i = 0;
  snprintf(buf, sizeof(buf), "You have the following on order: \r\n");
  for (struct shop_order_data *order = shop_table[shop_nr].order; order; order = order->next)
    if (order->player == GET_IDNUM(ch))
    {
      i++;
      float totaltime = order->timeavail - time(0);
      totaltime = totaltime / SECS_PER_MUD_DAY;
      int real_obj = real_object(order->item);
      if (real_obj >= 0)
        snprintf(ENDOF(buf), sizeof(buf) - strlen(buf), " %d) %-30s (%d) - ", i, GET_OBJ_NAME(&obj_proto[real_obj]), order->number);
      else
        strlcat(buf, " ERROR\r\n", sizeof(buf));
      if (totaltime < 0) {
        time_t time_left = order->expiration - time(0);
        int days = time_left / (60 * 60 * 24);
        time_left -= 60 * 60 * 24 * days;

        int hours = time_left / (60 * 60);
        time_left -= 60 * 60 * hours;

        int minutes = time_left / 60;

        if (days > 0)
          snprintf(buf3, sizeof(buf3), "in %d IRL day%s, %d hour%s", days, days != 1 ? "s" : "", hours, hours != 1 ? "s" : "");
        else if (hours > 0)
          snprintf(buf3, sizeof(buf3), "in %d IRL hour%s, %d minute%s", hours, hours != 1 ? "s" : "", minutes, minutes != 1 ? "s" : "");
        else if (minutes > 0)
          snprintf(buf3, sizeof(buf3), "in %d IRL minute%s", minutes, minutes != 1 ? "s" : "");
        else
          strlcpy(buf3, "at any moment", sizeof(buf3));
        snprintf(ENDOF(buf), sizeof(buf) - strlen(buf), " AVAILABLE (%d nuyen / ea; expires %s)\r\n", order->price - order->paid, buf3);
      }
      else if (totaltime < 1 && (int)(24 * totaltime) == 0)
        strlcat(buf, " less than one hour\r\n", sizeof(buf));
      else
        snprintf(ENDOF(buf), sizeof(buf) - strlen(buf), " %d %s\r\n", totaltime < 1 ? (int)(24 * totaltime) : (int)totaltime,
                totaltime < 1 ? "hours" : (totaltime == 1 ? "day" : "days"));
    }
  if (i == 0)
  {
    snprintf(buf, sizeof(buf), "%s You don't have anything on order here.", GET_CHAR_NAME(ch));
    do_say(keeper, buf, cmd_say, SCMD_SAYTO);
  } else
    send_to_char(buf, ch);
}


void shop_rec(char *arg, struct char_data *ch, struct char_data *keeper, vnum_t shop_nr)
{
  if (!is_open(keeper, shop_nr))
    return;
  if (!is_ok_char(keeper, ch, shop_nr))
    return;
  char buf[MAX_STRING_LENGTH];
  int number = atoi(arg);
	if (number <= 0) {
		send_to_char("Unrecognized selection. Syntax: RECEIVE [number].\r\n", ch);
		return;
	}
  for (struct shop_order_data *order = shop_table[shop_nr].order; order; order = order->next) {
    if (order->player == GET_IDNUM(ch) && order->timeavail < time(0) && !--number)
    {
      struct obj_data *obj = read_object(order->item, VIRTUAL), *cred = get_first_credstick(ch, "credstick");
      if (!cred && shop_table[shop_nr].type == SHOP_LEGAL) {
        if (access_level(ch, LVL_ADMIN)) {
          send_to_char(ch, "You stare unblinkingly at %s until %s makes an exception to the no-credstick, no-sale policy.\r\n",
                       GET_NAME(keeper), HSSH(keeper));
        } else {
          snprintf(buf, sizeof(buf), "%s No Credstick, No Sale.", GET_CHAR_NAME(ch));
          do_say(keeper, buf, cmd_say, SCMD_SAYTO);
          extract_obj(obj);
          return;
        }
      }
      shop_receive(ch, keeper, arg, order->number, cred && shop_table[shop_nr].type != SHOP_BLACK? 0 : 1, NULL, obj,
                       shop_table[shop_nr].type == SHOP_BLACK ? NULL : cred, order->price - order->paid, shop_nr, order);
      return;
    }
  }
  snprintf(buf, sizeof(buf), "%s I don't have anything for you.", GET_CHAR_NAME(ch));
  do_say(keeper, buf, cmd_say, SCMD_SAYTO);
}


void shop_cancel(char *arg, struct char_data *ch, struct char_data *keeper, vnum_t shop_nr)
{
  char buf[MAX_STRING_LENGTH];
  if (!is_open(keeper, shop_nr))
    return;
  if (!is_ok_char(keeper, ch, shop_nr))
    return;
  int number;
  strcpy(buf, GET_CHAR_NAME(ch));
  if (!(number = atoi(arg)))
    snprintf(ENDOF(buf), sizeof(buf) - strlen(buf), " What order do you want to cancel?");
  else
  {
    for (struct shop_order_data *order = shop_table[shop_nr].order; order; order = order->next)
      if (order->player == GET_IDNUM(ch) && !--number) {
        struct shop_order_data *temp;
        int real_obj = real_object(order->item);
        if (real_obj >= 0)
          snprintf(ENDOF(buf), sizeof(buf) - strlen(buf), " I'll let my contacts know you no longer want %s.", GET_OBJ_NAME(&obj_proto[real_obj]));
        else
          snprintf(ENDOF(buf), sizeof(buf) - strlen(buf), " I'll let my contacts know you no longer want that.");

        // Refund the prepayment, minus the usual fee.
        if (order->paid > 0) {
          int repayment_amount = order->paid - (order->paid / PREORDER_RESTOCKING_FEE_DIVISOR);
          if (repayment_amount > 0) {
            // In this instance, we do a raw refund, then decrement the shop purchase amount.
            GET_NUYEN_RAW(ch) += repayment_amount;
            GET_NUYEN_INCOME_THIS_PLAY_SESSION(ch, NUYEN_OUTFLOW_SHOP_PURCHASES) -= repayment_amount;
            act("$n hands $N some nuyen.", FALSE, keeper, 0, ch, TO_ROOM);
          }
        }

        REMOVE_FROM_LIST(order, shop_table[shop_nr].order, next);
        delete order;
        do_say(keeper, buf, cmd_say, SCMD_SAYTO);
        return;
      }
    snprintf(ENDOF(buf), sizeof(buf) - strlen(buf), " You don't have that many orders with me.");
  }
  do_say(keeper, buf, cmd_say, SCMD_SAYTO);

}

void shop_hours(struct char_data *ch, vnum_t shop_nr)
{
#ifdef USE_SHOP_OPEN_CLOSE_TIMES
  strcpy(buf, "This shop is ");
  if (!shop_table[shop_nr].open && shop_table[shop_nr].close == 24)
    strcat(buf, "always open");
  else {
    strcat(buf, "open from ");
    if (shop_table[shop_nr].open < 12)
      snprintf(ENDOF(buf), sizeof(buf) - strlen(buf), "%dam", shop_table[shop_nr].open);
    else if (shop_table[shop_nr].open == 12)
      strcat(buf, "noon");
    else if (shop_table[shop_nr].open == 24)
      strcat(buf, "midnight");
    else
      snprintf(ENDOF(buf), sizeof(buf) - strlen(buf), "%dpm", shop_table[shop_nr].open - 12);
    strcat(buf, " until ");
    if (shop_table[shop_nr].close < 12)
      snprintf(ENDOF(buf), sizeof(buf) - strlen(buf), "%dam", shop_table[shop_nr].close);
    else if (shop_table[shop_nr].close == 12)
      strcat(buf, "noon");
    else if (shop_table[shop_nr].close == 24)
      strcat(buf, "midnight");
    else
      snprintf(ENDOF(buf), sizeof(buf) - strlen(buf), "%dpm", shop_table[shop_nr].close - 12);
  }
  strcat(buf, ".\r\n");
  send_to_char(buf, ch);
#else
  send_to_char("The shop-hours system is disabled, so shops are always open.\r\n", ch);
#endif
}

SPECIAL(shop_keeper)
{
  struct char_data *keeper = (struct char_data *) me;
  vnum_t shop_nr;
  if (!cmd)
    return FALSE;
  for (shop_nr = 0; shop_nr <= top_of_shopt; shop_nr++)
    if (shop_table[shop_nr].keeper == GET_MOB_VNUM(keeper))
      break;
  if (shop_nr > top_of_shopt)
    return FALSE;

  skip_spaces(&argument);
  if (CMD_IS("buy"))
    shop_buy(argument, sizeof(argument), ch, keeper, shop_nr);
  else if (CMD_IS("sell"))
    shop_sell(argument, ch, keeper, shop_nr);
  else if (CMD_IS("list"))
    shop_list(argument, ch, keeper, shop_nr);
  else if (CMD_IS("info"))
    shop_info(argument, ch, keeper, shop_nr);
  else if (CMD_IS("value"))
    shop_value(argument, ch, keeper, shop_nr);
  else if (CMD_IS("check"))
    shop_check(argument, ch, keeper, shop_nr);
  else if (CMD_IS("receive") || CMD_IS("recieve"))
    shop_rec(argument, ch, keeper, shop_nr);
  else if (CMD_IS("hours"))
    shop_hours(ch, shop_nr);
  else if (CMD_IS("cancel"))
    shop_cancel(argument, ch, keeper, shop_nr);
  else if (CMD_IS("probe"))
    return shop_probe(argument, ch, keeper, shop_nr);
  else if (CMD_IS("install"))
    shop_install(argument, ch, keeper, shop_nr);
  else if (CMD_IS("uninstall"))
    shop_uninstall(argument, ch, keeper, shop_nr);
  else
    return FALSE;
  return TRUE;
}

void assign_shopkeepers(void)
{
  int index, rnum;
  cmd_say = find_command("say");
  for (index = 0; index <= top_of_shopt; index++) {
    if (shop_table[index].keeper <= 0)
      continue;
    if ((rnum = real_mobile(shop_table[index].keeper)) < 0)
      log_vfprintf("Shopkeeper #%d does not exist (shop #%d)",
          shop_table[index].keeper, shop_table[index].vnum);
    else if (mob_index[rnum].func != shop_keeper && shop_table[index].keeper != 1151) {
      mob_index[rnum].sfunc = mob_index[rnum].func;
      mob_index[rnum].func = shop_keeper;
    }
  }
}

void randomize_shop_prices(void)
{
  PERF_PROF_SCOPE(pr_, __func__);
  for (int i = 0; i <= top_of_shopt; i++) {
    if (shop_table[i].random_amount)
      shop_table[i].random_current = number(-shop_table[i].random_amount, shop_table[i].random_amount);
    else
      shop_table[i].random_current = 0;
    for (struct shop_sell_data *sell = shop_table[i].selling; sell; sell = sell->next)
      for (int q = 0; q < SHOP_LAST_IDNUM_LIST_SIZE; q++)
        sell->lastidnum[q] = 0;
  }
}

void list_detailed_shop(struct char_data *ch, vnum_t shop_nr)
{
  snprintf(buf, sizeof(buf), "Vnum:       [%5ld], Rnum: [%5ld]\r\n", shop_table[shop_nr].vnum, shop_nr);

  int real_mob = real_mobile(shop_table[shop_nr].keeper);
  if (real_mob > 0) {
    snprintf(ENDOF(buf), sizeof(buf) - strlen(buf), "Name: %30s Shopkeeper: %s [%5ld]\r\n", shop_table[shop_nr].shopname,
             mob_proto[real_mob].player.physical_text.name,
             shop_table[shop_nr].keeper);
  } else {
    snprintf(ENDOF(buf), sizeof(buf) - strlen(buf), "Name: %30s Shopkeeper: (N/A) [%5ld]\r\n", shop_table[shop_nr].shopname, shop_table[shop_nr].keeper);
  }

  snprintf(ENDOF(buf), sizeof(buf) - strlen(buf), "Buy at:     [%1.2f], Sell at: [%1.2f], +/- %%: [%d], Current %%: [%d]",
          shop_table[shop_nr].profit_buy, shop_table[shop_nr].profit_sell, shop_table[shop_nr].random_amount,
          shop_table[shop_nr].random_current);
#ifdef USE_SHOP_OPEN_CLOSE_TIMES
  snprintf(ENDOF(buf), sizeof(buf) - strlen(buf), ", Hours [%d-%d]\r\n", shop_table[shop_nr].open, shop_table[shop_nr].close);
#else
  snprintf(ENDOF(buf), sizeof(buf) - strlen(buf), ", Hours (disabled) [%d-%d]\r\n", shop_table[shop_nr].open, shop_table[shop_nr].close);
#endif
  snprintf(ENDOF(buf), sizeof(buf) - strlen(buf), "Type:       %s, Etiquette: %s\r\n", shop_type[shop_table[shop_nr].type], skills[shop_table[shop_nr].ettiquete].name);
  shop_table[shop_nr].races.PrintBits(buf2, MAX_STRING_LENGTH, pc_race_types, NUM_RACES);
  snprintf(ENDOF(buf), sizeof(buf) - strlen(buf), "!Serves:     %s\r\n", buf2);
  shop_table[shop_nr].flags.PrintBits(buf2, MAX_STRING_LENGTH, shop_flags, SHOP_FLAGS);
  snprintf(ENDOF(buf), sizeof(buf) - strlen(buf), "Flags:      %s\r\n", buf2);
  shop_table[shop_nr].buytypes.PrintBits(buf2, MAX_STRING_LENGTH, item_types, NUM_ITEMS);
  snprintf(ENDOF(buf), sizeof(buf) - strlen(buf), "Buytypes:   %s\r\n", buf2);
  strcat(buf, "Selling: \r\n");
  for (struct shop_sell_data *selling = shop_table[shop_nr].selling; selling; selling = selling->next) {
    int real_obj = real_object(selling->vnum);
    if (real_obj)
      snprintf(ENDOF(buf), sizeof(buf) - strlen(buf), "%-50s (%5ld) Type: %s Amount: %d\r\n", obj_proto[real_obj].text.name,
              selling->vnum, selling_type[selling->type], selling->stock);
  }
  page_string(ch->desc, buf, 0);
}

void write_shops_to_disk(int zone)
{
  long counter, realcounter;
  FILE *fp;
  struct shop_data *shop;
  zone = real_zone(zone);
  snprintf(buf, sizeof(buf), "%s/%d.shp", SHP_PREFIX, zone_table[zone].number);
  fp = fopen(buf, "w+");

  /* start running through all mobiles in this zone */
  for (counter = zone_table[zone].number * 100;
       counter <= zone_table[zone].top;
       counter++) {
    realcounter = real_shop(counter);

    if (realcounter >= 0) {
      shop = shop_table+realcounter;
      fprintf(fp, "#%ld\n", shop->vnum);
      fprintf(fp, "Keeper:\t%ld\n"
              "ProfitBuy:\t%.2f\n"
              "ProfitSell:\t%.2f\n"
              "Random:\t%d\n"
#ifdef USE_SHOP_OPEN_CLOSE_TIMES
              "Open:\t%d\n"
              "Close:\t%d\n"
#else
              "Open (disabled):\t%d\n"
              "Close (disabled):\t%d\n"
#endif
              "Type:\t%s\n",
              shop->keeper, shop->profit_buy, shop->profit_sell, shop->random_amount, shop->open,
              shop->close, shop_type[shop->type]);
      fprintf(fp, "NoSuchItemKeeper:\t%s\n"
              "NoSuchItemPlayer:\t%s\n"
              "NoNuyen:\t%s\n"
              "DoesntBuy:\t%s\n"
              "Buy:\t%s\n"
              "Sell:\t%s\n"
              "Name:\t%s\n",
              shop->no_such_itemk, shop->no_such_itemp, shop->not_enough_nuyen, shop->doesnt_buy, shop->buy, shop->sell, shop->shopname);
      fprintf(fp, "Flags:\t%s\n"
              "Races:\t%s\n"
              "Buytypes:\t%s\n"
              "Etiquette:\t%d\n",
              shop->flags.ToString(), shop->races.ToString(), shop->buytypes.ToString(), shop->ettiquete);
      fprintf(fp, "[SELLING]\n");
      int i = 0;
      for (struct shop_sell_data *sell = shop->selling; sell; sell = sell->next, i++) {
        fprintf(fp, "\t[SELL %d]\n"
                "\t\tVnum:\t%ld\n"
                "\t\tType:\t%s\n"
                "\t\tStock:\t%d\n",
                i, sell->vnum, selling_type[sell->type], sell->stock);
      }
      fprintf(fp, "BREAK\n");
    }
  }
  fprintf(fp, "END\n");
  fclose(fp);
  write_index_file("shp");
}

#define SHOP d->edit_shop
#define CH d->character
extern void free_shop(struct shop_data *shop);

void shedit_disp_race_menu(struct descriptor_data *d)
{
  CLS(CH);
  send_to_char("1) Human\r\n2) Dwarf\r\n3) Elf\r\n4) Ork\r\n5) Troll\r\n", CH);
  SHOP->races.PrintBits(buf, MAX_STRING_LENGTH, pc_race_types, NUM_RACES);
  send_to_char(CH, "Won't sell to: ^c%s^n\r\nEnter Race (0 to quit): ", buf);
  d->edit_mode = SHEDIT_RACE_MENU;
}

void shedit_disp_flag_menu(struct descriptor_data *d)
{
  CLS(CH);
  for (int i = 1; i < SHOP_FLAGS; i += 2)
  {
    send_to_char(CH, "%2d) %-20s %2d) %-20s\r\n",
                 i, shop_flags[i],
                 i + 1, i + 1 <= SHOP_FLAGS ?
                 shop_flags[i + 1] : "");
  }
  SHOP->flags.PrintBits(buf, MAX_STRING_LENGTH, shop_flags, SHOP_FLAGS);
  send_to_char(CH, "Flags: ^c%s^n\r\nEnter Flag (0 to quit): ", buf);
  d->edit_mode = SHEDIT_FLAG_MENU;
}

void shedit_disp_buytypes_menu(struct descriptor_data *d)
{
  CLS(CH);
  for (int counter = 1; counter < NUM_ITEMS; counter += 2)
  {
    send_to_char(CH, "%2d) %-20s %2d) %-20s\r\n",
                 counter, item_types[counter],
                 counter + 1, counter + 1 <= NUM_ITEMS ?
                 item_types[counter + 1] : "");
  }
  SHOP->buytypes.PrintBits(buf, MAX_STRING_LENGTH, item_types, NUM_ITEMS);
  send_to_char(CH, "Will Buy: ^c%s^n\r\nSelect Buytypes (0 to quit): ", buf);
  d->edit_mode = SHEDIT_BUYTYPES_MENU;
}

void shedit_disp_text_menu(struct descriptor_data *d)
{
  CLS(CH);
  send_to_char(CH, "1) Shop Name: ^c%s^n\r\n", SHOP->shopname);
  send_to_char(CH, "2) No Such Item (Keeper): ^c%s^n\r\n", SHOP->no_such_itemk);
  send_to_char(CH, "3) No Such Item (Player): ^c%s^n\r\n", SHOP->no_such_itemp);
  send_to_char(CH, "4) Not Enough Nuyen: ^c%s^n\r\n", SHOP->not_enough_nuyen);
  send_to_char(CH, "5) Doesn't Buy: ^c%s^n\r\n", SHOP->doesnt_buy);
  send_to_char(CH, "6) Buy: ^c%s^n\r\n", SHOP->buy);
  send_to_char(CH, "7) Sell: ^c%s^n\r\n", SHOP->sell);
  send_to_char(    "q) Return to Main Menu\r\nEnter Your Choice: ", CH);
  d->edit_mode = SHEDIT_TEXT_MENU;
}

void shedit_disp_selling_menu(struct descriptor_data *d)
{
  CLS(CH);
  int i = 1;
  for (struct shop_sell_data *sell = SHOP->selling; sell; sell = sell->next, i++)
  {
    int real_obj = real_object(sell->vnum);
    if (real_obj < 0)
      snprintf(buf, sizeof(buf), "%d) INVALID OBJECT, DELETE IT  ", i);
    else
      snprintf(buf, sizeof(buf), "%d) ^c%-50s^n (^c%5ld^n) Type: ^c%6s^n", i, GET_OBJ_NAME(&obj_proto[real_obj]),
              sell->vnum, selling_type[sell->type]);
    if (sell->type == SELL_STOCK)
      snprintf(ENDOF(buf), sizeof(buf) - strlen(buf), " Stock: ^c%d^n", sell->stock);
    strcat(buf, "\r\n");
    send_to_char(buf, CH);
  }
  send_to_char("a) Add Entry\r\nd) Delete Entry\r\nEnter Choice (0 to quit)", CH);
  d->edit_mode = SHEDIT_SELLING_MENU;
}

void shedit_disp_menu(struct descriptor_data *d)
{
  int real_mob = real_mobile(SHOP->keeper);
  CLS(CH);
  send_to_char(CH, "Shop Number: %ld\r\n", SHOP->vnum);
  send_to_char(CH, "1) Keeper: ^c%ld^n (^c%s^n)\r\n", SHOP->keeper,
               real_mob > 0 ? GET_NAME(&mob_proto[real_mob]) : "NULL");
  send_to_char(CH, "2) Shop Type: ^c%s^n\r\n", shop_type[SHOP->type]);
  send_to_char(CH, "3) Cost Multiplier when Player Buying: ^c%.2f^n\r\n", SHOP->profit_buy);
  send_to_char(CH, "4) Cost Multiplier when Player Selling: ^c%.2f^n\r\n", SHOP->profit_sell);
  send_to_char(CH, "5) %% +/-: ^c%d^n\r\n", SHOP->random_amount);
#ifdef USE_SHOP_OPEN_CLOSE_TIMES
  send_to_char(CH, "6) Opens: ^c%d^n Closes: ^c%d^n\r\n", SHOP->open, SHOP->close);
#else
  send_to_char(CH, "6) Opens: ^c%d^n Closes: ^c%d^n (Note: system is currently disabled)\r\n", SHOP->open, SHOP->close);
#endif
  send_to_char(CH, "7) Etiquette Used for Availability Rolls: ^c%s^n\r\n", skills[SHOP->ettiquete].name);
  SHOP->races.PrintBits(buf, MAX_STRING_LENGTH, pc_race_types, NUM_RACES);
  send_to_char(CH, "8) Doesn't Trade With: ^c%s^n\r\n", buf);
  SHOP->flags.PrintBits(buf, MAX_STRING_LENGTH, shop_flags, SHOP_FLAGS);
  send_to_char(CH, "9) Flags: ^c%s^n\r\n", buf);
  send_to_char("a) Buytype Menu\r\n", CH);
  send_to_char("b) Text Menu\r\n", CH);
  send_to_char("c) Selling Menu\r\n", CH);
  send_to_char("q) Quit and save\r\n", CH);
  send_to_char("x) Exit and abort\r\n", CH);
  send_to_char("Enter your choice:\r\n", CH);
  d->edit_mode = SHEDIT_MAIN_MENU;
}

void shedit_parse(struct descriptor_data *d, const char *arg)
{
  int number = atoi(arg);
  float profit;
  rnum_t shop_nr, i;
  switch(d->edit_mode)
  {
  case SHEDIT_CONFIRM_EDIT:
    switch (*arg) {
    case 'y':
    case 'Y':
      shedit_disp_menu(d);
      break;
    case 'n':
    case 'N':
      STATE(d) = CON_PLAYING;
      free_shop(SHOP);
      delete d->edit_shop;
      d->edit_shop = NULL;
      d->edit_number = 0;
      PLR_FLAGS(d->character).RemoveBit(PLR_EDITING);
      break;
    default:
      send_to_char("That's not a valid choice!\r\n", CH);
      send_to_char("Do you wish to edit it?\r\n", CH);
      break;
    }
    break;
  case SHEDIT_CONFIRM_SAVESTRING:
    switch(*arg) {
    case 'y':
    case 'Y':
      if (!vnum_from_non_connected_zone(d->edit_number)) {
        snprintf(buf, sizeof(buf),"%s wrote new shop #%ld",
                GET_CHAR_NAME(d->character), d->edit_number);
        mudlog(buf, d->character, LOG_WIZLOG, TRUE);
      }
      shop_nr = real_shop(d->edit_number);
      if (shop_nr > 0) {
        rnum_t okn, nkn;
        if (shop_table[shop_nr].keeper != SHOP->keeper && shop_table[shop_nr].keeper != 1151) {
          okn = real_mobile(shop_table[shop_nr].keeper);
          nkn = real_mobile(SHOP->keeper);
          if (okn > 0 && mob_index[okn].func == shop_keeper) {
            mob_index[okn].func = mob_index[okn].sfunc;
            mob_index[okn].sfunc = NULL;
          } else if (okn > 0 && mob_index[okn].sfunc == shop_keeper)
            mob_index[okn].sfunc = NULL;
          if (nkn > 0) {
            mob_index[nkn].sfunc = mob_index[nkn].func;
            mob_index[nkn].func = shop_keeper;
          }
        }
        SHOP->order = shop_table[shop_nr].order;
        SHOP->vnum = d->edit_number;
        free_shop(shop_table + shop_nr);
        shop_table[shop_nr] = *SHOP;
      } else {
        vnum_t counter, counter2;
        bool found = FALSE;
        for (counter = 0; counter <= top_of_shopt; counter++) {
          if (shop_table[counter].vnum > d->edit_number) {
            for (counter2 = top_of_shopt + 1; counter2 > counter; counter2--)
              shop_table[counter2] = shop_table[counter2 - 1];

            SHOP->vnum = d->edit_number;
            shop_table[counter] = *(d->edit_shop);
            shop_nr = counter;
            found = TRUE;
            break;
          }
        }
        if (!found) {
          SHOP->vnum = d->edit_number;
          shop_table[++top_of_shopt] = *SHOP;
          shop_nr = top_of_shopt;
        }
      }
      i = real_mobile(shop_table[shop_nr].keeper);
      if (i > 0 && shop_table[shop_nr].keeper != 1151) {
        mob_index[i].sfunc = mob_index[i].func;
        mob_index[i].func = shop_keeper;
      }
      write_shops_to_disk(CH->player_specials->saved.zonenum);
      delete d->edit_shop;
      d->edit_shop = NULL;
      d->edit_number = 0;
      STATE(d) = CON_PLAYING;
      PLR_FLAGS(d->character).RemoveBit(PLR_EDITING);
      send_to_char("Done.\r\n", d->character);
      break;
    case 'n':
    case 'N':
      send_to_char("Shop not saved, aborting.\r\n", d->character);
      STATE(d) = CON_PLAYING;
      free_shop(SHOP);
      delete d->edit_shop;
      d->edit_shop = NULL;
      d->edit_number = 0;
      d->edit_number2 = 0;
      PLR_FLAGS(d->character).RemoveBit(PLR_EDITING);
      break;
    default:
      send_to_char("Invalid choice!\r\n", d->character);
      send_to_char("Do you wish to save this shop internally?\r\n", d->character);
      break;
    }
    break;
  case SHEDIT_MAIN_MENU:
    switch (*arg) {
    case '1':
      send_to_char(CH, "Enter Shopkeeper vnum: ");
      d->edit_mode = SHEDIT_KEEPER;
      break;
    case '2':
      CLS(CH);
      send_to_char(CH, "0) Grey (nuyen and credstick; applies street index)\r\n1) Legal (credstick only; no street index)\r\n2) Black (nuyen only; applies street index)\r\nEnter Shop Type: ");
      d->edit_mode = SHEDIT_TYPE;
      break;
    case '3':
      send_to_char(CH, "Enter multiplier for buy command: ");
      d->edit_mode = SHEDIT_PROFIT_BUY;
      break;
    case '4':
      send_to_char(CH, "Enter multiplier for sell command: ");
      d->edit_mode = SHEDIT_PROFIT_SELL;
      break;
    case '5':
      send_to_char(CH, "Enter maximum price deviation: ");
      d->edit_mode = SHEDIT_RANDOM;
      break;
    case '6':
      send_to_char(CH, "Enter opening time: ");
      d->edit_mode = SHEDIT_OPEN;
      break;
    case '7':
      CLS(CH);
      send_to_char(CH, "1) Corporate Etiquette\r\n"
                   "2) Media Etiquette\r\n"
                   "3) Street Etiquette\r\n"
                   "4) Tribal Etiquette\r\n"
                   "5) Elf Etiquette\r\n"
                   "Enter Etiquette skill required for availability tests: ");
      d->edit_mode = SHEDIT_ETTI;
      break;
    case '8':
      shedit_disp_race_menu(d);
      break;
    case '9':
      shedit_disp_flag_menu(d);
      break;
    case 'a':
    case 'A':
      shedit_disp_buytypes_menu(d);
      break;
    case 'b':
    case 'B':
      shedit_disp_text_menu(d);
      break;
    case 'c':
    case 'C':
      shedit_disp_selling_menu(d);
      break;
    case 'q':
    case 'Q':
      d->edit_mode = SHEDIT_CONFIRM_SAVESTRING;
      shedit_parse(d, "y");
      break;
    case 'x':
    case 'X':
      d->edit_mode = SHEDIT_CONFIRM_SAVESTRING;
      shedit_parse(d,"n");
      break;
    }
    break;
  case SHEDIT_ETTI:
    if (number < 1 || number > 5) {
      send_to_char("Invalid Choice! Enter Etiquette skill: ", CH);
      return;
    }
    SHOP->ettiquete = --number + SKILL_CORPORATE_ETIQUETTE;
    shedit_disp_menu(d);
    break;
  case SHEDIT_KEEPER:
    SHOP->keeper = number;
    shedit_disp_menu(d);
    break;
  case SHEDIT_TYPE:
    if (number < 0 || number > 2) {
      send_to_char("Invalid Choice! Enter Shop Type: ", CH);
      return;
    }
    SHOP->type = number;
    shedit_disp_menu(d);
    break;
  case SHEDIT_PROFIT_BUY:
    profit = atof(arg);
    if (profit < 1) {
      send_to_char("Buy price multiplier must be at least 1! Enter multiplier: ", CH);
      return;
    }
    SHOP->profit_buy = profit;
    shedit_disp_menu(d);
    break;
  case SHEDIT_PROFIT_SELL:
    profit = atof(arg);
    if (profit > 1 || profit <= 0) {
      send_to_char("Sell price multiplier must be greater than 0 and no more than 1! Enter multiplier: ", CH);
      return;
    }
    SHOP->profit_sell = profit;
    shedit_disp_menu(d);
    break;
  case SHEDIT_RANDOM:
    if (number < 0) {
      send_to_char("Invalid Amount! Enter Maximum Price Deviation: ", CH);
      return;
    }
    SHOP->random_amount = number;
    shedit_disp_menu(d);
    break;
  case SHEDIT_OPEN:
    if (number < 0 || number > 24) {
      send_to_char("Invalid Time! Enter Opening Time (Between 0 and 24): ", CH);
      return;
    }
    SHOP->open = number;
    send_to_char("Enter Closing Time: ", CH);
    d->edit_mode = SHEDIT_OPEN2;
    break;
  case SHEDIT_OPEN2:
    if (number < 0 || number > 24) {
      send_to_char("Invalid Time! Enter Closing Time (Between 0 and 24): ", CH);
      return;
    }
    SHOP->close = number;
    shedit_disp_menu(d);
    break;
  case SHEDIT_RACE_MENU:
    switch (*arg) {
    case '0':
      shedit_disp_menu(d);
      break;
    case '1':
    case '2':
    case '3':
    case '4':
    case '5':
      number++;
      SHOP->races.ToggleBit(number);
      shedit_disp_race_menu(d);
      break;
    default:
      shedit_disp_race_menu(d);
      break;
    }
    break;
  case SHEDIT_FLAG_MENU:
    if (number == 0) {
      shedit_disp_menu(d);
      return;
    } else if (number > 0 && number < SHOP_FLAGS)
      SHOP->flags.ToggleBit(number);
    shedit_disp_flag_menu(d);
    break;
  case SHEDIT_BUYTYPES_MENU:
    if (number == 0) {
      shedit_disp_menu(d);
      return;
    } else if (number > 0 && number < NUM_ITEMS)
      SHOP->buytypes.ToggleBit(number);
    shedit_disp_buytypes_menu(d);
    break;
  case SHEDIT_TEXT_MENU:
    switch (*arg) {
    case 'q':
    case 'Q':
      shedit_disp_menu(d);
      break;
    case '1':
      send_to_char("Enter name of shop: ", CH);
      d->edit_mode = SHEDIT_SHOPNAME;
      break;
    case '2':
      send_to_char("Enter no such item (Keeper) message: ", CH);
      d->edit_mode = SHEDIT_NSIK;
      break;
    case '3':
      send_to_char("Enter no such item (Player) message: ", CH);
      d->edit_mode = SHEDIT_NSIP;
      break;
    case '4':
      send_to_char("Enter not enough nuyen message: ", CH);
      d->edit_mode = SHEDIT_NEN;
      break;
    case '5':
      send_to_char("Enter doesn't buy message: ", CH);
      d->edit_mode = SHEDIT_NOBUY;
      break;
    case '6':
      send_to_char("Enter buying message (%d for nuyen value): ", CH);
      d->edit_mode = SHEDIT_BUYMSG;
      break;
    case '7':
      send_to_char("Enter selling message: (%d for nuyen value): ", CH);
      d->edit_mode = SHEDIT_SELLMSG;
      break;
    }
    break;
  case SHEDIT_NSIK:
    if (SHOP->no_such_itemk)
      delete [] SHOP->no_such_itemk;
    SHOP->no_such_itemk = str_dup(arg);
    shedit_disp_text_menu(d);
    break;
  case SHEDIT_NSIP:
    if (SHOP->no_such_itemp)
      delete [] SHOP->no_such_itemp;
    SHOP->no_such_itemp = str_dup(arg);
    shedit_disp_text_menu(d);
    break;
  case SHEDIT_NEN:
    if (SHOP->not_enough_nuyen)
      delete [] SHOP->not_enough_nuyen;
    SHOP->not_enough_nuyen = str_dup(arg);
    shedit_disp_text_menu(d);
    break;
  case SHEDIT_NOBUY:
    if (SHOP->doesnt_buy)
      delete [] SHOP->doesnt_buy;
    SHOP->doesnt_buy = str_dup(arg);
    shedit_disp_text_menu(d);
    break;
  case SHEDIT_BUYMSG:
    if (SHOP->buy)
      delete [] SHOP->buy;
    SHOP->buy = str_dup(arg);
    shedit_disp_text_menu(d);
    break;
  case SHEDIT_SELLMSG:
    if (SHOP->sell)
      delete [] SHOP->sell;
    SHOP->sell = str_dup(arg);
    shedit_disp_text_menu(d);
    break;
  case SHEDIT_SHOPNAME:
    if (SHOP->shopname)
      delete [] SHOP->shopname;
    SHOP->shopname = str_dup(arg);
    shedit_disp_text_menu(d);
    break;
  case SHEDIT_SELLING_MENU:
    switch (*arg) {
    case '0':
      shedit_disp_menu(d);
      break;
    case 'a':
    case 'A':
      send_to_char("Enter Item VNum to sell: ", CH);
      d->edit_mode = SHEDIT_SELL_ADD;
      break;
    case 'd':
    case 'D':
      send_to_char("Delete which entry: ", CH);
      d->edit_mode = SHEDIT_SELL_DELETE;
      break;
    }
    break;
  case SHEDIT_SELL_ADD:
    if (number > 0) {
      struct shop_sell_data *sell = new shop_sell_data;
      sell->vnum = number;
      sell->next = SHOP->selling;
      sell->stock = 0;
      SHOP->selling = sell;
      CLS(CH);
      send_to_char("0) Always\r\n1) Availability Code\r\n2) Limited Stock\r\nEnter Supply Type: ", CH);
      d->edit_mode = SHEDIT_SELL_ADD1;
    } else
      send_to_char("Invalid VNum! What Item VNum: ", CH);
    break;
  case SHEDIT_SELL_ADD1:
    if (number >= 0 && number <= 2) {
      SHOP->selling->type = number;
      if (number == SELL_STOCK) {
        send_to_char("How many in stock: ", CH);
        d->edit_mode = SHEDIT_SELL_ADD2;
      } else
        shedit_disp_selling_menu(d);
    } else
      send_to_char("Invalid Type! Enter Supply Type: ", CH);
    break;
  case SHEDIT_SELL_ADD2:
    if (number > 0) {
      SHOP->selling->stock = number;
      shedit_disp_selling_menu(d);
    } else
      send_to_char("Must be stocking more than 0! How many in stock: ", CH);
    break;
  case SHEDIT_SELL_DELETE:
    if (number > 0) {
      for (struct shop_sell_data *sell = SHOP->selling; number && sell; sell = sell->next) {
        number--;
        if (!number) {
          struct shop_sell_data *temp;
          REMOVE_FROM_LIST(sell, SHOP->selling, next);
          delete [] sell;
          break;
        }
      }
      shedit_disp_selling_menu(d);
    } else
      shedit_disp_selling_menu(d);
    break;
  }
}

bool can_sell_object(struct obj_data *obj, struct char_data *keeper, int shop_nr) {
  if (!obj) {
    snprintf(buf2, sizeof(buf2), "Shop %ld ('%s'): Hiding nonexistant item from sale.", shop_table[shop_nr].vnum, GET_NAME(keeper));
    mudlog(buf2, keeper, LOG_SYSLOG, TRUE);
    return FALSE;
  }

  // Pre-compose our rejection string.
  snprintf(buf2, sizeof(buf2), "Shop %ld ('%s'): Hiding %s (%ld) from sale due to ",
           shop_table[shop_nr].vnum,
           keeper ? GET_NAME(keeper) : "masked",
           GET_OBJ_NAME(obj),
           GET_OBJ_VNUM(obj));

  // Don't allow sale of forbidden vnums.
  if (GET_OBJ_VNUM(obj) == OBJ_OLD_BLANK_MAGAZINE_FROM_CLASSIC
      || GET_OBJ_VNUM(obj) == OBJ_BLANK_MAGAZINE) {
    strlcat(buf2, "matching a forbidden vnum.", sizeof(buf2));
    mudlog(buf2, keeper, LOG_SYSLOG, TRUE);
    extract_obj(obj);
    return FALSE;
  }

  // Don't allow sale of zero-cost items.
  if (GET_OBJ_COST(obj) < 1) {
    snprintf(ENDOF(buf2), sizeof(buf2) - strlen(buf2), "cost of %d.", GET_OBJ_COST(obj));
    mudlog(buf2, keeper, LOG_SYSLOG, TRUE);
    extract_obj(obj);
    return FALSE;
  }

  // Checks based on item type.
  switch (GET_OBJ_TYPE(obj)) {
    // Don't allow sale of NERP spell formulae.
    case ITEM_SPELL_FORMULA:
      if (spell_is_nerp(GET_SPELLFORMULA_SPELL(obj))) {
        snprintf(ENDOF(buf2), sizeof(buf2) - strlen(buf2), "having NERP spell %s.", spells[GET_OBJ_VAL(obj, 1)].name);
        mudlog(buf2, keeper, LOG_SYSLOG, TRUE);
        extract_obj(obj);
        return FALSE;
      }
      break;
    /*
    case ITEM_FIREWEAPON:
    case ITEM_MISSILE:
      strlcat(buf, "being a fireweapon or fireweapon ammo.", sizeof(buf));
      mudlog(buf2, keeper, LOG_SYSLOG, TRUE);
      extract_obj(obj);
      return FALSE;
    */
  }

  // Can sell it.
  return TRUE;
}

void shop_install(char *argument, struct char_data *ch, struct char_data *keeper, vnum_t shop_nr) {
  struct obj_data *obj;

  // Non-docs won't install things.
  if (!shop_table[shop_nr].flags.IsSet(SHOP_DOCTOR)) {
    do_say(keeper, "Hold on now, I'm not a doctor! Find someone else to install your 'ware.", cmd_say, 0);
    return;
  }

  if (!access_level(ch, LVL_ADMIN) && !(CAN_SEE(keeper, ch))) {
    do_say(keeper, "How am I supposed to work on someone I can't see?", cmd_say, 0);
    return;
  }

  if (IS_PROJECT(ch) || IS_NPC(ch)) {
    send_to_char("You're having a hard time getting the shopkeeper's attention.\r\n", ch);
    return;
  }

  argument = one_argument(argument, arg);

  if (!*arg) {
    send_to_char(ch, "What 'ware you want the shopkeeper to install?\r\n");
    return;
  }

  int dotmode = find_all_dots(arg);

  /* Can't junk or donate all */
  if ((dotmode == FIND_ALL) || dotmode == FIND_ALLDOT) {
    send_to_char(ch, "You'll have to install one thing at a time.\r\n");
    return;
  }

  if (!(obj = get_obj_in_list_vis(ch, arg, ch->carrying))) {
    send_to_char(ch, "You don't seem to have %s %s in your inventory.\r\n", AN(arg), arg);
    return;
  }

  if (GET_OBJ_TYPE(obj) != ITEM_SHOPCONTAINER) {
    send_to_char(ch, "Shopkeepers can only install 'ware from packages, and %s doesn't qualify.\r\n", GET_OBJ_NAME(obj));
    return;
  }

  if (!obj->contains) {
    send_to_char(ch, "%s is empty!\r\n", capitalize(GET_OBJ_NAME(obj)));
    snprintf(buf, sizeof(buf), "SYSERR: Shop container '%s' is empty!", GET_OBJ_NAME(obj));
    mudlog(buf, ch, LOG_SYSLOG, TRUE);
    return;
  }

  obj = obj->contains;

  // We charge 1/X of the price of the thing to install it, up to the configured maximum value.
  int install_cost = MIN(CYBERWARE_INSTALLATION_COST_MAXIMUM, GET_OBJ_COST(obj) / CYBERWARE_INSTALLATION_COST_FACTOR);

  // Try to deduct the install cost from their credstick.
  struct obj_data *cred = get_first_credstick(ch, "credstick");
  if (!cred || install_cost > GET_ITEM_MONEY_VALUE(cred))
    cred = NULL;

  if (!cred && install_cost > GET_NUYEN(ch)) {
    snprintf(buf, sizeof(buf), "%s I'd charge %d nuyen to install that. Come back when you've got the cash.", GET_CHAR_NAME(ch), install_cost);
    do_say(keeper, buf, cmd_say, SCMD_SAYTO);
    return;
  }

  // Chargen shops should never see an install command like this, so we automatically assume you're getting injured.
  if (install_ware_in_target_character(obj, keeper, ch, TRUE)) {
    snprintf(buf, sizeof(buf), "%s That'll be %d nuyen.", GET_CHAR_NAME(ch), install_cost);
    do_say(keeper, buf, cmd_say, SCMD_SAYTO);

    // Success! Deduct the cost from your payment method.
    if (cred)
      lose_nuyen_from_credstick(ch, cred, install_cost, NUYEN_OUTFLOW_SHOP_PURCHASES);
    else
      lose_nuyen(ch, install_cost, NUYEN_OUTFLOW_SHOP_PURCHASES);
  }
}

void shop_uninstall(char *argument, struct char_data *ch, struct char_data *keeper, vnum_t shop_nr) {
  struct obj_data *obj;

  // Non-docs won't uninstall things.
  if (!shop_table[shop_nr].flags.IsSet(SHOP_DOCTOR)) {
    do_say(keeper, "Hold on now, I'm not a doctor! Find someone else to uninstall your 'ware.", cmd_say, 0);
    return;
  }

  if (!access_level(ch, LVL_ADMIN) && !(CAN_SEE(keeper, ch))) {
    do_say(keeper, "How am I supposed to work on someone I can't see?", cmd_say, 0);
    return;
  }

  if (IS_PROJECT(ch) || IS_NPC(ch)) {
    send_to_char("You're having a hard time getting the shopkeeper's attention.\r\n", ch);
    return;
  }

  argument = one_argument(argument, arg);

  if (!*arg) {
    send_to_char(ch, "What 'ware you want the shopkeeper to uninstall?\r\n");
    return;
  }

  int dotmode = find_all_dots(arg);

  /* Can't junk or donate all */
  if ((dotmode == FIND_ALL) || dotmode == FIND_ALLDOT) {
    send_to_char(ch, "You'll have to uninstall one thing at a time.\r\n");
    return;
  }

  if (!(obj = get_obj_in_list_vis(ch, arg, ch->cyberware)) && !(obj = get_obj_in_list_vis(ch, arg, ch->bioware))) {
    send_to_char(ch, "You don't seem to have %s %s installed.\r\n", AN(arg), arg);
    return;
  }

  if (GET_OBJ_TYPE(obj) != ITEM_BIOWARE && GET_OBJ_TYPE(obj) != ITEM_CYBERWARE) {
    send_to_char(ch, "Shopkeepers can only uninstall 'ware.\r\n", GET_OBJ_NAME(obj));
    return;
  }

  // We charge 1/X of the price of the thing to install it, up to the configured maximum value.
  int uninstall_cost = MIN(CYBERWARE_INSTALLATION_COST_MAXIMUM, GET_OBJ_COST(obj) / CYBERWARE_INSTALLATION_COST_FACTOR);

  // Try to deduct the install cost from their credstick.
  struct obj_data *cred = get_first_credstick(ch, "credstick");
  if (!cred || uninstall_cost > GET_ITEM_MONEY_VALUE(cred))
    cred = NULL;

  if (!cred && uninstall_cost > GET_NUYEN(ch)) {
    snprintf(buf, sizeof(buf), "%s I'd charge %d nuyen to uninstall that. Come back when you've got the cash.", GET_CHAR_NAME(ch), uninstall_cost);
    do_say(keeper, buf, cmd_say, SCMD_SAYTO);
    return;
  }

  // Chargen shops should never see an install command like this, so we automatically assume you're getting injured.
  if (uninstall_ware_from_target_character(obj, keeper, ch, TRUE)) {
    snprintf(buf, sizeof(buf), "%s That'll be %d nuyen.", GET_CHAR_NAME(ch), uninstall_cost);
    do_say(keeper, buf, cmd_say, SCMD_SAYTO);

    // Success! Deduct the cost from your payment method.
    if (cred)
      lose_nuyen_from_credstick(ch, cred, uninstall_cost, NUYEN_OUTFLOW_SHOP_PURCHASES);
    else
      lose_nuyen(ch, uninstall_cost, NUYEN_OUTFLOW_SHOP_PURCHASES);

    // Package it up and hand it over.
    if (GET_OBJ_COST(obj) > 0) {
      struct obj_data *shop_container = shop_package_up_ware(obj);
      act("$n packages up $N's old 'ware and hands it to $M.", TRUE, keeper, NULL, ch, TO_NOTVICT);
      act("$n packages up your old 'ware and hands it to you.", TRUE, keeper, NULL, ch, TO_VICT);
      obj_to_char(shop_container, ch);
    } else {
      snprintf(buf, sizeof(buf), "%s Sorry, %s was too damaged to be worth reusing.", GET_CHAR_NAME(ch), GET_OBJ_NAME(obj));
      do_say(keeper, buf, cmd_say, SCMD_SAYTO);
      extract_obj(obj);
      obj = NULL;
    }
  }
}

struct obj_data *shop_package_up_ware(struct obj_data *obj) {
  struct obj_data *shop_container = read_object(OBJ_SHOPCONTAINER, VIRTUAL);
  GET_OBJ_BARRIER(shop_container) = 32;
  GET_OBJ_MATERIAL(shop_container) = MATERIAL_ADV_PLASTICS;
  GET_OBJ_COST(shop_container) = 0;

  snprintf(buf3, sizeof(buf3), "a packaged-up '%s'", GET_OBJ_NAME(obj));
  DELETE_ARRAY_IF_EXTANT(shop_container->restring);
  shop_container->restring = str_dup(buf3);

  obj_to_obj(obj, shop_container);
  return shop_container;
}

void save_shop_orders() {
  PERF_PROF_SCOPE(pr_, __func__);
  FILE *fl;
  float totaltime = 0;
  time_t curr_time = time(0);

  for (int shop_nr = 0; shop_nr <= top_of_shopt; shop_nr++) {
    // Wipe the existing shop order save files-- they're out of date.
    snprintf(buf, sizeof(buf), "order/%ld", shop_table[shop_nr].vnum);
    unlink(buf);

    if (shop_table[shop_nr].order) {
      // Expire out orders that have reached their end of life. Yes, this means a whole separate for-loop just for this.
      struct shop_order_data *next_order, *temp;
      for (struct shop_order_data *order = shop_table[shop_nr].order; order; order = next_order) {
        next_order = order->next;
        totaltime = order->expiration - curr_time;
        if (totaltime <= 0) {
          // Notify them about the expiry, but only for orders with a prepay-- this prevents the 7-day spamstorm when this change is first launched.
          if (order->paid > 0) {
            int repayment_amount = order->paid - (order->paid / PREORDER_RESTOCKING_FEE_DIVISOR);
            int real_obj = real_object(order->item);
            snprintf(buf2, sizeof(buf2), "%s can't be held for you any longer at %s. %d nuyen will be refunded to your account.\r\n",
                     real_obj > 0 ? CAP(obj_proto[real_obj].text.name) : "Something",
                     shop_table[shop_nr].shopname,
                     repayment_amount
                    );
            int real_mob = real_mobile(shop_table[shop_nr].keeper);
            if (real_mob > 0)
              raw_store_mail(order->player, 0, mob_proto[real_mob].player.physical_text.name, (const char *) buf2);
            else
              raw_store_mail(order->player, 0, "An anonymous shopkeeper", (const char *) buf2);

            // Wire the funds. This will not notify them (they're already getting a message through here.)
            // This is a thorny one-- this is technically a sink, since we're losing X% of the refunded value, but the PC may not be online.
            // We'll leave this as an invisible sink for now.
            if (repayment_amount > 0)
              wire_nuyen(NULL, repayment_amount, order->player);
          }

          // Remove the order from the list, then delete it.
          REMOVE_FROM_LIST(order, shop_table[shop_nr].order, next);
          delete order;
        }
      }

      // Since we potentially wiped all the shop orders, we have to check if we have any others-- no sense writing an empty file.
      if (!shop_table[shop_nr].order)
        continue;

      // We have orders, so open the shop file and write the header.
      if (!(fl = fopen(buf, "w"))) {
        perror("SYSERR: Error saving order file");
        continue;
      }
      int i = 0;
      fprintf(fl, "[ORDERS]\n");

      // Iterate through the orders, writing them each to file. Also, send a mail if the order is ready to be picked up.
      for (struct shop_order_data *order = shop_table[shop_nr].order; order; order = order->next, i++) {
        totaltime = order->timeavail - time(0);
        if (!order->sent && totaltime < 0) {
          int real_obj = real_object(order->item);
          snprintf(buf2, sizeof(buf2), "%s has arrived at %s and is ready for pickup for a total cost of %d nuyen. It will be held for you for %d days.\r\n",
                   real_obj > 0 ? CAP(obj_proto[real_obj].text.name) : "Something",
                   shop_table[shop_nr].shopname,
                   order->price,
                   PREORDERS_ARE_GOOD_FOR_X_DAYS
                  );
          int real_mob = real_mobile(shop_table[shop_nr].keeper);
          if (real_mob > 0)
            raw_store_mail(order->player, 0, mob_proto[real_mob].player.physical_text.name, (const char *) buf2);
          else
            raw_store_mail(order->player, 0, "An anonymous shopkeeper", (const char *) buf2);
          order->sent = TRUE;
        }
        fprintf(fl, "\t[ORDER %d]\n", i);
        fprintf(fl, "\t\tItem:\t%ld\n"
                "\t\tPlayer:\t%ld\n"
                "\t\tTime:\t%d\n"
                "\t\tNumber:\t%d\n"
                "\t\tPrice:\t%d\n"
                "\t\tSent:\t%d\n"
                "\t\tPaid:\t%d\n"
                "\t\tExpiration:\t%ld\n", order->item, order->player, order->timeavail, order->number, order->price, order->sent, order->paid, order->expiration);
      }
      fclose(fl);
    }
  }
}
