#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <time.h>

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

extern struct time_info_data time_info;
extern struct obj_data *get_first_credstick(struct char_data *ch, const char *arg);
extern const char *pc_race_types[];
extern void reduce_abilities(struct char_data *vict);
ACMD_CONST(do_say);
int cmd_say;

const char *shop_flags[] =
  {
    "Nothing",
    "Doctor",
    "!NEGOTIATE",
    "!RESELL"
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
  char buf[MAX_STRING_LENGTH];
  buf[0] = '\0';
  if (shop_table[shop_nr].open > shop_table[shop_nr].close) {
    if (time_info.hours < shop_table[shop_nr].open && time_info.hours > shop_table[shop_nr].close)
      sprintf(buf, "We're not open yet.");
  } else {
    if (time_info.hours < shop_table[shop_nr].open)
      sprintf(buf, "We're not open yet.");
    else if (time_info.hours > shop_table[shop_nr].close)
      sprintf(buf, "We've closed for the day.");
  }
  if (!*buf)
    return TRUE;
  else
  {
    do_say(keeper, buf, cmd_say, 0);
    return FALSE;
  }
}

bool is_ok_char(struct char_data * keeper, struct char_data * ch, vnum_t shop_nr)
{
  char buf[400];

  if (!access_level(ch, LVL_ADMIN)
      && !(CAN_SEE(keeper, ch)))
  {
    do_say(keeper, "I don't trade with someone I can't see.", cmd_say, 0);
    return FALSE;
  }
  if (IS_PROJECT(ch))
    return FALSE;
  if (IS_NPC(ch) || access_level(ch, LVL_BUILDER))
    return TRUE;
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
    sprintf(buf, "%s We don't sell to your type here.", GET_CHAR_NAME(ch));
    do_say(keeper, buf, cmd_say, SCMD_SAYTO);
    return FALSE;
  }
  return TRUE;
}

int buy_price(struct obj_data *obj, vnum_t shop_nr)
{
  int i = (int)(GET_OBJ_COST(obj) * shop_table[shop_nr].profit_buy);
  i += (int)((i * shop_table[shop_nr].random_current)/ 100);
  return i;
}

int sell_price(struct obj_data *obj, vnum_t shop_nr)
{
  int i = (int)(GET_OBJ_COST(obj) * shop_table[shop_nr].profit_sell);
  i += (int)((i * shop_table[shop_nr].random_current)/ 100);
  return i;
}


int transaction_amt(char *arg)
{
  int num;
  one_argument(arg, buf);
  if (*buf)
    if ((is_number(buf))) {
      num = atoi(buf);
      strcpy(arg, arg + strlen(buf) + 1);
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
    int num = atoi(arg+1) - 1;
    for (;sell; sell = sell->next, num--) {
      if (obj_proto[real_object(sell->vnum)].obj_flags.cost < 1)
        num++;
      if (num == 0)
        break;
    }
    if (sell)
      *obj = read_object(sell->vnum, VIRTUAL);
  } else
  {
    for (; sell; sell = sell->next)
      if (obj_proto[real_object(sell->vnum)].obj_flags.cost &&
          (isname(arg, obj_proto[real_object(sell->vnum)].text.name) ||
           isname(arg, obj_proto[real_object(sell->vnum)].text.keywords))) {
        *obj = read_object(sell->vnum, VIRTUAL);
        break;
      }
  }
  return sell;
}

bool shop_receive(struct char_data *ch, struct char_data *keeper, char *arg, int buynum, bool cash,
                  struct shop_sell_data *sell, struct obj_data *obj, struct obj_data *cred, int price, vnum_t shop_nr)
{
  char buf[MAX_STRING_LENGTH], buf2[MAX_STRING_LENGTH];
  strcpy(buf, GET_CHAR_NAME(ch));
  int bought = 0;
  if (sell && sell->type == SELL_STOCK && sell->stock < 1)
  {
    sprintf(ENDOF(buf), " That item isn't currently available.");
    do_say(keeper, buf, cmd_say, SCMD_SAYTO);
    return FALSE;
  }
  if ((cred && GET_OBJ_VAL(cred, 0) < price) || (!cred && GET_NUYEN(ch) < price))
  {
    sprintf(ENDOF(buf), " %s", shop_table[shop_nr].not_enough_nuyen);
    do_say(keeper, buf, cmd_say, SCMD_SAYTO);
    return FALSE;
  }
  sprintf(buf2, "You now have %s.", GET_OBJ_NAME(obj));

  if (shop_table[shop_nr].flags.IsSet(SHOP_DOCTOR))
  {
    struct obj_data *check;
    if (buynum != 1) {
      sprintf(ENDOF(buf), " You can only have one of those installed!");
      do_say(keeper, buf, cmd_say, SCMD_SAYTO);
      return FALSE;
    }
    switch (GET_OBJ_TYPE(obj)) {
      case ITEM_CYBERWARE:
        for (struct obj_data *bio = ch->bioware; bio; bio = bio->next_content)
          if (!biocyber_compatibility(obj, bio, ch))
             return FALSE;
      case ITEM_BIOWARE:
        for (struct obj_data *cyber = ch->cyberware; cyber; cyber = cyber->next_content)
          if (!biocyber_compatibility(obj, cyber, ch))
             return FALSE;
    }
    if (GET_OBJ_TYPE(obj) == ITEM_CYBERWARE) {
      if (ch->real_abils.ess + ch->real_abils.esshole < (GET_TOTEM(ch) == TOTEM_EAGLE ?
                                GET_OBJ_VAL(obj, 4) << 1 : GET_OBJ_VAL(obj, 4))) {
        sprintf(ENDOF(buf), " That operation would kill you!");
        do_say(keeper, buf, cmd_say, SCMD_SAYTO);
        return FALSE;
      }
      int enhancers = 0;
      for (check = ch->cyberware; check != NULL; check = check->next_content) {
        if ((GET_OBJ_VNUM(check) == GET_OBJ_VNUM(obj))) {
          if (GET_OBJ_VAL(check, 0) == CYB_REACTIONENHANCE) {
            if (++enhancers == 6) {
              sprintf(ENDOF(buf), " You already have the maximum number of reaction enhancers installed.");
              do_say(keeper, buf, cmd_say, SCMD_SAYTO);
              return FALSE;
            }
          } else {
            sprintf(ENDOF(buf), " You already have that installed.");
            do_say(keeper, buf, cmd_say, SCMD_SAYTO);
            return FALSE;
          }
        }
        if (GET_OBJ_VAL(obj, 0) != CYB_EYES && GET_OBJ_VAL(obj, 0) != CYB_FILTRATION
            && GET_OBJ_VAL(obj, 0) != CYB_REACTIONENHANCE && GET_OBJ_VAL(check, 0) == GET_OBJ_VAL(obj, 0)) {
          sprintf(ENDOF(buf), " You already have a similar piece of cyberware.");
          do_say(keeper, buf, cmd_say, SCMD_SAYTO);
          return FALSE;
        }
      }
      int esscost = GET_OBJ_VAL(obj, 4);
      if (GET_TOTEM(ch) == TOTEM_EAGLE)
        esscost *= 2;
      if (ch->real_abils.esshole < esscost) {
        esscost = esscost - ch->real_abils.esshole;
        ch->real_abils.esshole = 0;
        ch->real_abils.ess -= esscost;
        if (GET_TRADITION(ch) != TRAD_MUNDANE)
          magic_loss(ch, esscost, TRUE);
      } else ch->real_abils.esshole -= esscost;
      obj_to_cyberware(obj, ch);
    } else if (GET_OBJ_TYPE(obj) == ITEM_BIOWARE) {
      int esscost = GET_OBJ_VAL(obj, 4); 
      if (GET_INDEX(ch) + esscost > 900) {
        sprintf(ENDOF(buf), " That operation would kill you!");
        do_say(keeper, buf, cmd_say, SCMD_SAYTO);
        return FALSE;
      }
      if ((GET_OBJ_VAL(obj, 0) == BIO_PATHOGENICDEFENSE || GET_OBJ_VAL(obj, 0) == BIO_TOXINEXTRACTOR) && 
          GET_OBJ_VAL(obj, 1) > GET_REAL_BOD(ch) / 2) {
        send_to_char("Your body cannot support pathogenic defense of that level.\r\n", ch);
        return FALSE;
      }
      for (check = ch->bioware; check; check = check->next_content) {
        if ((GET_OBJ_VNUM(check) == GET_OBJ_VNUM(obj))) {
          sprintf(ENDOF(buf), " You already have that installed.");
          do_say(keeper, buf, cmd_say, SCMD_SAYTO);
          return FALSE;
        }
        if (GET_OBJ_VAL(check, 0) == GET_OBJ_VAL(obj, 0)) {
          sprintf(ENDOF(buf), " You already have a similar piece of bioware.");
          do_say(keeper, buf, cmd_say, SCMD_SAYTO);
          return FALSE;
        }
      }
      GET_INDEX(ch) += esscost;
      if (GET_INDEX(ch) > ch->real_abils.highestindex) {
        if (GET_TRADITION(ch) != TRAD_MUNDANE) {
          int change = GET_INDEX(ch) - ch->real_abils.highestindex;
          change /= 2;
          magic_loss(ch, change, TRUE);
        }
        ch->real_abils.highestindex = GET_INDEX(ch);
      }
      obj_to_bioware(obj, ch);
    }
    act("$n takes out a sharpened scalpel and lies $N down on the operating table.",
        FALSE, keeper, 0, ch, TO_NOTVICT);
    sprintf(buf, "%s Relax...this won't hurt a bit.", GET_CHAR_NAME(ch));
    do_say(keeper, buf, cmd_say, SCMD_SAYTO);
    GET_PHYSICAL(ch) = 100;
    GET_MENTAL(ch) = 100;
    act("You delicately install $p into $N's body.",
        FALSE, keeper, obj, ch, TO_CHAR);
    act("$n performs a delicate procedure on $N.",
        FALSE, keeper, 0, ch, TO_NOTVICT);
    act("$n delicately installs $p into your body.",
        FALSE, keeper, obj, ch, TO_VICT);
    bought = 1;
    if (cred)
      GET_OBJ_VAL(cred, 0) -= price;
    else
      GET_NUYEN(ch) -= price;
    if (sell) {
      if (sell->type == SELL_BOUGHT && !--sell->stock) {
        struct shop_sell_data *temp;
        REMOVE_FROM_LIST(sell, shop_table[shop_nr].selling, next);
        delete sell;
        sell = NULL;
      } else if (sell->type == SELL_STOCK)
        sell->stock--;
    }
    if (GET_BIOOVER(ch) > 0)
      send_to_char("You don't feel too well.\r\n", ch);
  } else
  {
    if (IS_CARRYING_N(ch) + 1 > CAN_CARRY_N(ch)) {
      send_to_char("You can't carry any more items.\r\n", ch);
      return FALSE;
    }
    if (IS_CARRYING_W(ch) + GET_OBJ_WEIGHT(obj) > CAN_CARRY_W(ch)) {
      send_to_char("It weighs too much!\r\n", ch);
      return FALSE;
    }
    if ((GET_OBJ_TYPE(obj) == ITEM_DECK_ACCESSORY && GET_OBJ_VAL(obj, 0) == TYPE_PARTS) ||
        (GET_OBJ_TYPE(obj) == ITEM_MAGIC_TOOL && GET_OBJ_VAL(obj, 0) == TYPE_SUMMONING)) {
      while (bought < buynum && (cred ? GET_OBJ_VAL(cred, 0) : GET_NUYEN(ch)) >= price) {
        if (cred)
          GET_OBJ_VAL(cred, 0) -= price;
        else
          GET_NUYEN(ch) -= price;
        bought++;
      }
      GET_OBJ_COST(obj) = GET_OBJ_COST(obj) * bought;
      struct obj_data *orig = ch->carrying;
      for (; orig; orig = orig->next_content)
        if (GET_OBJ_TYPE(obj) == GET_OBJ_TYPE(orig) && GET_OBJ_VAL(obj, 0) == GET_OBJ_VAL(orig, 0) &&
            GET_OBJ_VAL(obj, 1) == GET_OBJ_VAL(orig, 1))
          break;
      if (orig) {
        GET_OBJ_COST(orig) += GET_OBJ_COST(obj);
        extract_obj(obj);
      } else
        obj_to_char(obj, ch);
    } else
      while(obj && (bought < buynum && IS_CARRYING_N(ch) < CAN_CARRY_N(ch) && IS_CARRYING_W(ch) +
                    GET_OBJ_WEIGHT(obj) <= CAN_CARRY_W(ch) && (cred ? GET_OBJ_VAL(cred, 0) : GET_NUYEN(ch)) >= price)) {
        if (GET_OBJ_VNUM(obj) == 17513 || GET_OBJ_VNUM(obj) == 62100)
          GET_OBJ_VAL(obj, 0) = GET_IDNUM(ch);
        obj_to_char(obj, ch);
        bought++;
        if (sell) {
          obj = NULL;
          if (sell->type == SELL_BOUGHT) {
            sell->stock--;
            if (sell->stock == 0) {
              struct shop_sell_data *temp;
              REMOVE_FROM_LIST(sell, shop_table[shop_nr].selling, next);
              delete sell;
              sell = NULL;
            } else
              obj = read_object(sell->vnum, VIRTUAL);
          } else if (sell->type == SELL_STOCK) {
            sell->stock--;
            if (sell->stock > 0)
              obj = read_object(sell->vnum, VIRTUAL);
          } else
            obj = read_object(sell->vnum, VIRTUAL);
        } else {
          obj = read_object(obj->item_number, REAL);
        }
        if (cred)
          GET_OBJ_VAL(cred, 0) -= price;
        else
          GET_NUYEN(ch) -= price;
      }
    if (bought < buynum) {
      strcpy(buf, GET_CHAR_NAME(ch));
      if (cash ? GET_NUYEN(ch) : GET_OBJ_VAL(cred, 0) < price)
        sprintf(ENDOF(buf), " You can only afford %d.", bought);
      else if (IS_CARRYING_N(ch) >= CAN_CARRY_N(ch))
        sprintf(ENDOF(buf), " You can only carry %d.", bought);
      else if (GET_OBJ_WEIGHT(ch->carrying) + IS_CARRYING_W(ch) > CAN_CARRY_W(ch))
        sprintf(ENDOF(buf), " You can only carry %d.", bought);
      do_say(keeper, buf, cmd_say, SCMD_SAYTO);
    }
  }
  sprintf(arg, shop_table[shop_nr].buy, bought * price);
  sprintf(buf, "%s %s", GET_CHAR_NAME(ch), arg);
  do_say(keeper, buf, cmd_say, SCMD_SAYTO);
  if (bought > 1)
    sprintf(ENDOF(buf2), " (x%d)\r\n", bought);
  send_to_char(buf2, ch);
  send_to_char("\r\n", ch);
  return TRUE;
}

void shop_buy(char *arg, struct char_data *ch, struct char_data *keeper, vnum_t shop_nr)
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
  if ((buynum = transaction_amt(arg)) < 0)
  {
    sprintf(buf, "%s A negative amount?  Try selling me something.", GET_CHAR_NAME(ch));
    do_say(keeper, buf, cmd_say, SCMD_SAYTO);
    return;
  }
  if (!(sell = find_obj_shop(arg, shop_nr, &obj)))
  {
    sprintf(buf, "%s %s", GET_CHAR_NAME(ch), shop_table[shop_nr].no_such_itemk);
    do_say(keeper, buf, cmd_say, SCMD_SAYTO);
    return;
  }
  one_argument(arg, buf);
  if (!str_cmp(buf, "cash"))
  {
    arg = any_one_arg(arg, buf);
    skip_spaces(&arg);
    cash = TRUE;
  } else if (!cred)
  {
    if (shop_table[shop_nr].type == SHOP_LEGAL) {
      sprintf(buf, "%s No Credstick, No Sale.", GET_CHAR_NAME(ch));
      do_say(keeper, buf, cmd_say, SCMD_SAYTO);
      return;
    }
    send_to_char("Lacking an activated credstick, you choose to deal in cash.\r\n", ch );
    cash = TRUE;
  } else if (shop_table[shop_nr].type == SHOP_BLACK)
  {
    send_to_char("The shopkeeper refuses to deal with credsticks.\r\n", ch);
    cash = TRUE;
  }
  if (!*arg || !buynum)
  {
    sprintf(buf, "%s What do you want to buy?", GET_CHAR_NAME(ch));
    do_say(keeper, buf, cmd_say, SCMD_SAYTO);
    return;
  }
  price = buy_price(obj, shop_nr);
  int bprice = price / 10;
  if (!shop_table[shop_nr].flags.IsSet(SHOP_WONT_NEGO))
    price = negotiate(ch, keeper, 0, price, 0, TRUE);
  if (sell->type == SELL_AVAIL && GET_AVAIL_OFFSET(ch))
    price += bprice * GET_AVAIL_OFFSET(ch);
  if (sell->type == SELL_AVAIL && GET_OBJ_AVAILTN(obj) > 0)
  {
    for (int q = 0; q <= 4; q++)
      if (sell->lastidnum[q] == GET_IDNUM(ch)) {
        sprintf(buf, "%s Sorry, I couldn't get that in for you.", GET_CHAR_NAME(ch));
        do_say(keeper, buf, cmd_say, SCMD_SAYTO);
        extract_obj(obj);
        return;
      }
    int target = GET_OBJ_AVAILTN(obj) - GET_AVAIL_OFFSET(ch);
    target = MAX(0, target - GET_POWER(ch, ADEPT_KINESICS));
    if (GET_RACE(ch) != GET_RACE(keeper))
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
    int skill = get_skill(ch, shop_table[shop_nr].ettiquete, target);
    for (struct obj_data *bio = ch->bioware; bio; bio = bio->next_content)
      if (GET_OBJ_VAL(bio, 0) == BIO_TAILOREDPHEREMONES) {
        skill += GET_OBJ_VAL(bio, 2) ? GET_OBJ_VAL(bio, 1) * 2: GET_OBJ_VAL(bio, 1);
        break;
      }
    int success = success_test(skill, target);
    if (success < 1 || buynum > 50) {
      sprintf(buf, "%s I can't get ahold of that one for a while.", GET_CHAR_NAME(ch));
      do_say(keeper, buf, cmd_say, SCMD_SAYTO);
      for (int q = 4; q >= 1; q--)
        sell->lastidnum[q] = sell->lastidnum[q-1];
      sell->lastidnum[0] = GET_IDNUM(ch);
    } else {
      float totaltime = ((GET_OBJ_AVAILDAY(obj) * buynum) / success) + (2 * GET_AVAIL_OFFSET(ch));
      sprintf(buf, "%s That will take about %d %s to come in.", GET_CHAR_NAME(ch),
              totaltime < 1 ? (int)(24 * totaltime) : (int)totaltime, totaltime < 1 ? "hours" : (totaltime == 1 ? "day" : "days"));
      do_say(keeper, buf, cmd_say, SCMD_SAYTO);
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
        struct shop_order_data *order = new shop_order_data;
        order->item = sell->vnum;
        order->player = GET_IDNUM(ch);
        order->timeavail = time(0) + (int)(SECS_PER_MUD_DAY * totaltime);
        order->number = buynum;
        order->price = price;
        order->next = shop_table[shop_nr].order;
        shop_table[shop_nr].order = order;
      }
    }
    extract_obj(obj);
  } else
  {
    shop_receive(ch, keeper, arg, buynum, cash, sell, obj, shop_table[shop_nr].type == SHOP_BLACK ? NULL : cred, price, shop_nr);
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
    send_to_char(ch, "What item do you want to sell?\r\n");
    return;
  }
  strcpy(buf, GET_CHAR_NAME(ch));
  if (shop_table[shop_nr].flags.IsSet(SHOP_DOCTOR))
  {
    if (!(obj = get_obj_in_list_vis(ch, arg, ch->cyberware))
        && !(obj = get_obj_in_list_vis(ch, arg, ch->bioware))) {
      sprintf(ENDOF(buf), " %s", shop_table[shop_nr].no_such_itemp);
      do_say(keeper, buf, cmd_say, SCMD_SAYTO);
      return;
    }
  } else
  {
    if (!(obj = get_obj_in_list_vis(ch, arg, ch->carrying))) {
      sprintf(ENDOF(buf), " %s", shop_table[shop_nr].no_such_itemp);
      do_say(keeper, buf, cmd_say, SCMD_SAYTO);
      return;
    }
  }
  if (!shop_table[shop_nr].buytypes.IsSet(GET_OBJ_TYPE(obj)) || IS_OBJ_STAT(obj, ITEM_NOSELL) || GET_OBJ_COST(obj) < 1)
  {
    sprintf(ENDOF(buf), " %s", shop_table[shop_nr].doesnt_buy);
    do_say(keeper, buf, cmd_say, SCMD_SAYTO);
    return;
  }
  if (shop_table[shop_nr].type == SHOP_LEGAL && !cred)
  {
    sprintf(ENDOF(buf), " No cred, no business.");
    do_say(keeper, buf, cmd_say, SCMD_SAYTO);
    return;
  }
  int sellprice = sell_price(obj, shop_nr);
  if (!shop_table[shop_nr].flags.IsSet(SHOP_WONT_NEGO))
    sellprice = negotiate(ch, keeper, 0, sellprice, 0, 0);
  if (shop_table[shop_nr].flags.IsSet(SHOP_DOCTOR))
  {
    if (GET_OBJ_TYPE(obj) == ITEM_BIOWARE) {
      obj_from_bioware(obj);
      GET_INDEX(ch) -= GET_OBJ_VAL(obj, 4);
    } else {
      obj_from_cyberware(obj);
      ch->real_abils.esshole += GET_OBJ_VAL(obj, 4);
    }
    act("$n takes out a sharpened scalpel and lies $N down on the operating table.",
        FALSE, keeper, 0, ch, TO_NOTVICT);
    sprintf(buf, "%s Relax...this won't hurt a bit.", GET_CHAR_NAME(ch));
    do_say(keeper, buf, cmd_say, SCMD_SAYTO);
    act("You delicately remove $p from $N's body.",
        FALSE, keeper, obj, ch, TO_CHAR);
    act("$n performs a delicate procedure on $N.",
        FALSE, keeper, 0, ch, TO_NOTVICT);
    act("$n delicately removes $p from your body.",
        FALSE, keeper, obj, ch, TO_VICT);
    GET_MENTAL(ch) = 100;
    GET_PHYSICAL(ch) = 100;
    affect_total(ch);
    if (GET_OBJ_VAL(obj, 0) == CYB_MEMORY)
      for (struct obj_data *chip = obj->contains; chip; chip = chip->next_content)
        if (GET_OBJ_VAL(chip, 9))
          ch->char_specials.saved.skills[GET_OBJ_VAL(chip, 0)][1] = 0;
  } else
    obj_from_char(obj);


  if (!cred || shop_table[shop_nr].type == SHOP_BLACK)
    GET_NUYEN(ch) += sellprice;
  else
    GET_OBJ_VAL(cred, 0) += sellprice;
  sprintf(buf3, "%s sold %s at %s (%ld) for %d.", GET_CHAR_NAME(ch), GET_OBJ_NAME(obj), GET_CHAR_NAME(keeper), shop_table[shop_nr].vnum, sellprice);
  mudlog(buf3, ch, LOG_GRIDLOG, TRUE);
  sprintf(arg, shop_table[shop_nr].sell, sellprice);
  sprintf(ENDOF(buf), " %s", arg);
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
}

void shop_list(char *arg, struct char_data *ch, struct char_data *keeper, vnum_t shop_nr)
{
  char buf[MAX_STRING_LENGTH];
  if (!is_open(keeper, shop_nr))
    return;
  if (!is_ok_char(keeper, ch, shop_nr))
    return;
  struct obj_data *obj;
  int i = 1;
  if (shop_table[shop_nr].flags.IsSet(SHOP_DOCTOR))
  {
    strcpy(buf, " ##   Available   Item                                Rating Ess/Index    Price\r\n"
                "-------------------------------------------------------------------------------\r\n");
    for (struct shop_sell_data *sell = shop_table[shop_nr].selling; sell; sell = sell->next, i++) {
      obj = read_object(sell->vnum, VIRTUAL);
      if (!obj || GET_OBJ_COST(obj) < 1) {
        i--;
        if (obj)
          extract_obj(obj);
        continue;
      }
      sprintf(ENDOF(buf), " %2d)  ", i);
      if (sell->type == SELL_ALWAYS || (sell->type == SELL_AVAIL && GET_OBJ_AVAILDAY(obj) == 0))
        sprintf(ENDOF(buf), "Yes         ");
      else if (sell->type == SELL_AVAIL) {
        if (GET_OBJ_AVAILDAY(obj) < 1)
          sprintf(ENDOF(buf), "~%-2d Hours   ", (int)(24 * GET_OBJ_AVAILDAY(obj)));
        else
          sprintf(ENDOF(buf), "~%-2d Day%c    ", (int)GET_OBJ_AVAILDAY(obj), GET_OBJ_AVAILDAY(obj) > 1 ? 's' : ' ');
      } else {
        if (sell->stock <= 0)
          sprintf(ENDOF(buf), "Out Of Stock");
        else
          sprintf(ENDOF(buf), "%-3d         ", sell->stock);
      }
      if (GET_OBJ_VAL(obj, 1) > 0)
        sprintf(buf2, "%d", GET_OBJ_VAL(obj, 1));
      else strcpy(buf2, "-");
      sprintf(ENDOF(buf), "%-33s^n %-6s%2s   %0.2f%c  %9d\r\n", GET_OBJ_NAME(obj),
              GET_OBJ_TYPE(obj) == ITEM_CYBERWARE ? "Cyber" : "Bio", buf2, ((float)GET_OBJ_VAL(obj, 4) / 100),
              GET_OBJ_TYPE(obj) == ITEM_CYBERWARE ? 'E' : 'I', buy_price(obj, shop_nr));
      extract_obj(obj);
    }
  } else
  {
    strcpy(buf, " ##   Available   Item                                              Price\r\n"
            "------------------------------------------------------------------------------\r\n");
    for (struct shop_sell_data *sell = shop_table[shop_nr].selling; sell; sell = sell->next, i++) {
      obj = read_object(sell->vnum, VIRTUAL);
      if (!obj || GET_OBJ_COST(obj) < 1) {
        i--;
        if (obj)
          extract_obj(obj);
        continue;
      }
      sprintf(ENDOF(buf), " %2d)  ", i);
      if (sell->type == SELL_ALWAYS || (sell->type == SELL_AVAIL && GET_OBJ_AVAILDAY(obj) == 0))
        sprintf(ENDOF(buf), "Yes         ");
      else if (sell->type == SELL_AVAIL) {
        if (GET_OBJ_AVAILDAY(obj) < 1)
          sprintf(ENDOF(buf), "~%-2d Hours   ", (int)(24 * GET_OBJ_AVAILDAY(obj)));
        else
          sprintf(ENDOF(buf), "~%-2d Day%c    ", (int)GET_OBJ_AVAILDAY(obj), GET_OBJ_AVAILDAY(obj) > 1 ? 's' : ' ');
      } else {
        if (sell->stock <= 0)
          sprintf(ENDOF(buf), "Out Of Stock");
        else
          sprintf(ENDOF(buf), "%-3d         ", sell->stock);
      }
        sprintf(ENDOF(buf), "%-48s^n %6d\r\n", GET_OBJ_NAME(obj),
                  buy_price(obj, shop_nr));
      if (strlen(buf) >= MAX_STRING_LENGTH - 200)
        break;
      extract_obj(obj);
    }
  }
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
  if (!*arg)
  {
    send_to_char(ch, "What item do you want valued?\r\n");
    return;
  }
  if (shop_table[shop_nr].flags.IsSet(SHOP_DOCTOR))
  {
    if (!(obj = get_obj_in_list_vis(ch, arg, ch->cyberware))
        || !(obj = get_obj_in_list_vis(ch, arg, ch->bioware))) {
      send_to_char(ch, "You don't seem to have that item.\r\n");
      return;
    }
  } else
  {
    if (!(obj = get_obj_in_list_vis(ch, arg, ch->carrying))) {
      send_to_char(ch, "You don't seem to have that item.\r\n");
      return;
    }
  }
  strcpy(buf, GET_CHAR_NAME(ch));
  if (!shop_table[shop_nr].buytypes.IsSet(GET_OBJ_TYPE(obj)) || IS_OBJ_STAT(obj, ITEM_NOSELL))
    strcat(buf, " I wouldn't buy that off of you.");
  else
    sprintf(ENDOF(buf), " I would be able to give you around %d nuyen for that.", sell_price(obj, shop_nr));

  do_say(keeper, buf, cmd_say, SCMD_SAYTO);
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
  if (!find_obj_shop(arg, shop_nr, &obj))
  {
    sprintf(buf, "%s I don't have that item.", GET_CHAR_NAME(ch));
    do_say(keeper, buf, cmd_say, SCMD_SAYTO);
    return;
  }
  sprintf(buf, "%s %s is", GET_CHAR_NAME(ch), CAP(obj->text.name));
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
      sprintf(ENDOF(buf), " %s", weapon_type[GET_OBJ_VAL(obj, 3)]);
      if (IS_OBJ_STAT(obj, ITEM_TWOHANDS))
        strcat(buf, " and requires two hands to wield correctly");
      if (GET_OBJ_VAL(obj, 7) > 0 || GET_OBJ_VAL(obj, 8) > 0 || GET_OBJ_VAL(obj, 9) > 0)
        strcat(buf, ". It comes standard with ");
      if (real_object(GET_OBJ_VAL(obj, 7)) > 0) {
        strcat(buf, obj_proto[real_object(GET_OBJ_VAL(obj, 7))].text.name);
        if ((GET_OBJ_VAL(obj, 8) > 0 && GET_OBJ_VAL(obj, 9) < 1) || (GET_OBJ_VAL(obj, 8) < 1 && GET_OBJ_VAL(obj, 9) > 0))
          strcat(buf, " and ");
        if (GET_OBJ_VAL(obj, 8) > 0 && GET_OBJ_VAL(obj, 9) > 0)
          strcat(buf, ", ");
      }
      if (real_object(GET_OBJ_VAL(obj, 8)) > 0) {
        strcat(buf, obj_proto[real_object(GET_OBJ_VAL(obj, 8))].text.name);
        if (GET_OBJ_VAL(obj, 9) > 0)
          strcat(buf, " and ");
      }
      if (real_object(GET_OBJ_VAL(obj, 9)) > 0)
        strcat(buf, obj_proto[real_object(GET_OBJ_VAL(obj, 9))].text.name);
      sprintf(ENDOF(buf), ". It can hold a maximum of %d rounds.", GET_OBJ_VAL(obj, 5));
    } else {}
    break;
  case ITEM_WORN:
    num = (GET_OBJ_VAL(obj, 5) > 20 ? (int)(GET_OBJ_VAL(obj, 5) / 100) : GET_OBJ_VAL(obj, 5)) +
          (GET_OBJ_VAL(obj, 6) > 20 ? (int)(GET_OBJ_VAL(obj, 6) / 100) : GET_OBJ_VAL(obj, 6));
    if (num < 1)
      strcat(buf, " a piece of clothing");
    else if (num < 4)
      strcat(buf, " a lightly armoured piece of clothing");
    else if (num < 7)
      strcat(buf, " a piece of light armour");
    else if (num < 10)
      strcat(buf, " a moderately rated piece of armour");
    else
      strcat(buf, " a piece of heavy armour");
    if (GET_OBJ_VAL(obj, 1) > 5)
      strcat(buf, " designed for carrying ammunition");
    else if (GET_OBJ_VAL(obj, 4) > 3 && GET_OBJ_VAL(obj, 4) < 6)
      strcat(buf, " that can carry a bit of gear");
    else if (GET_OBJ_VAL(obj, 4) >= 6)
      strcat(buf, " that can carry alot of gear");
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
    sprintf(ENDOF(buf), " a rating %d %s program, that is %dMp in size.", GET_OBJ_VAL(obj, 1),
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
    strcat(buf, " a docwagon contract, it will call them out when your vital signs drop.");
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
      strcat(buf, " piece of furniture.");
    break;
  case ITEM_DECK_ACCESSORY:
    if (GET_OBJ_VAL(obj, 0) == TYPE_COOKER) {
      strcat(buf, " a");
      if (GET_OBJ_VAL(obj, 0) <= 2)
        strcat(buf, " sluggish");
      else if (GET_OBJ_VAL(obj, 0) <= 5)
        strcat(buf, " quick");
      else if (GET_OBJ_VAL(obj, 0) <= 8)
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
    sprintf(ENDOF(buf), " a rating %d spell formula, describing %s. It is designed for use by a %s mage.", GET_OBJ_VAL(obj, 0),
            spells[GET_OBJ_VAL(obj, 1)].name, GET_OBJ_VAL(obj, 2) == 1 ? "shamanic" : "hermetic");
    break;
  case ITEM_GUN_ACCESSORY:
    sprintf(ENDOF(buf), " a firearm accessory that attaches to the %s of a weapon.", (GET_OBJ_VAL(obj, 0) == 0 ? "top" : 
                                                                                     (GET_OBJ_VAL(obj, 0) == 1 ? "barrel" : "bottom")));
    break;
  case ITEM_GUN_AMMO:
    sprintf(ENDOF(buf), " a box of ammunition for reloading %s magazines.", weapon_type[GET_OBJ_VAL(obj, 1)]);
    break;
  case ITEM_FOCUS:
    sprintf(ENDOF(buf), " a rating %d %s focus.", GET_OBJ_VAL(obj, 1), foci_type[GET_OBJ_VAL(obj, 0)]);
    break;
  case ITEM_MAGIC_TOOL:
    if (GET_OBJ_VAL(obj, 0) == TYPE_LIBRARY_CONJURE || GET_OBJ_VAL(obj, 0) == TYPE_LIBRARY_SPELL) {
      sprintf(ENDOF(buf), "  is a rating %d ", GET_OBJ_VAL(obj, 1));
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
    sprintf(buf, "%s I don't know anything about that.", GET_CHAR_NAME(ch));
  }
  strcat(buf, " It weighs about ");
  if (GET_OBJ_WEIGHT(obj) < 1) {
    sprintf(ENDOF(buf), "%d grams", (int)(GET_OBJ_WEIGHT(obj) * 1000));
  } else sprintf(ENDOF(buf), "%.0f kilogram%s", GET_OBJ_WEIGHT(obj), (GET_OBJ_WEIGHT(obj) >= 2 ? "s" : ""));
  sprintf(ENDOF(buf), " and I couldn't let it go for less than %d nuyen.", buy_price(obj, shop_nr));
  do_say(keeper, buf, cmd_say, SCMD_SAYTO);
}

void shop_check(char *arg, struct char_data *ch, struct char_data *keeper, vnum_t shop_nr)
{
  char buf[MAX_STRING_LENGTH];
  if (!is_open(keeper, shop_nr))
    return;
  if (!is_ok_char(keeper, ch, shop_nr))
    return;
  int i = 0;
  sprintf(buf, "You have the following on order: \r\n");
  for (struct shop_order_data *order = shop_table[shop_nr].order; order; order = order->next)
    if (order->player == GET_IDNUM(ch))
    {
      i++;
      float totaltime = order->timeavail - time(0);
      totaltime = totaltime / SECS_PER_MUD_DAY;
      sprintf(ENDOF(buf), " %d) %-30s (%d) - ", i, GET_OBJ_NAME(&obj_proto[real_object(order->item)]), order->number);
      if (totaltime < 0)
        strcat(buf, " AVAILABLE\r\n");
      else
        sprintf(ENDOF(buf), " %d %s\r\n", totaltime < 1 ? (int)(24 * totaltime) : (int)totaltime,
                totaltime < 1 ? "hours" : (totaltime == 1 ? "day" : "days"));
    }
  if (i == 0)
  {
    sprintf(buf, "%s You don't have anything on order here.", GET_CHAR_NAME(ch));
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
  if (number == 0)
    number = 1;
  for (struct shop_order_data *order = shop_table[shop_nr].order; order; order = order->next)
    if (order->player == GET_IDNUM(ch) && order->timeavail < time(0) && !--number)
    {
      struct obj_data *obj = read_object(order->item, VIRTUAL), *cred = get_first_credstick(ch, "credstick");
      if (!cred && shop_table[shop_nr].type == SHOP_LEGAL) {
        sprintf(buf, "%s No Credstick, No Sale.", GET_CHAR_NAME(ch));
        do_say(keeper, buf, cmd_say, SCMD_SAYTO);
        return;
      }
      if (shop_receive(ch, keeper, arg, order->number, cred && shop_table[shop_nr].type != SHOP_BLACK? 0 : 1, NULL, obj,
                       shop_table[shop_nr].type == SHOP_BLACK ? NULL : cred, order->price, shop_nr)) {
        struct shop_order_data *temp;
        REMOVE_FROM_LIST(order, shop_table[shop_nr].order, next);
        delete order;
      }
      return;
    }
  sprintf(buf, "%s I don't have anything for you.", GET_CHAR_NAME(ch));
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
    sprintf(ENDOF(buf), " What order do you want to cancel?");
  else
  {
    for (struct shop_order_data *order = shop_table[shop_nr].order; order; order = order->next)
      if (order->player == GET_IDNUM(ch) && !--number) {
        struct shop_order_data *temp;
        sprintf(ENDOF(buf), " I'll let my contacts know you no longer want %s.", GET_OBJ_NAME(&obj_proto[real_object(order->item)]));
        REMOVE_FROM_LIST(order, shop_table[shop_nr].order, next);
        delete order;
        do_say(keeper, buf, cmd_say, SCMD_SAYTO);
        return;
      }
    sprintf(ENDOF(buf), " You don't have that many orders with me.");
  }
  do_say(keeper, buf, cmd_say, SCMD_SAYTO);

}

void shop_hours(struct char_data *ch, vnum_t shop_nr)
{
  strcpy(buf, "This shop is ");
  if (!shop_table[shop_nr].open && shop_table[shop_nr].close == 24)
    strcat(buf, "always open");
  else {
    strcat(buf, "open from ");
    if (shop_table[shop_nr].open < 12)
      sprintf(ENDOF(buf), "%dam", shop_table[shop_nr].open);
    else if (shop_table[shop_nr].open == 12)
      strcat(buf, "noon");
    else if (shop_table[shop_nr].open == 24)
      strcat(buf, "midnight");
    else
      sprintf(ENDOF(buf), "%dpm", shop_table[shop_nr].open - 12);
    strcat(buf, " until ");
    if (shop_table[shop_nr].close < 12)
      sprintf(ENDOF(buf), "%dam", shop_table[shop_nr].close);
    else if (shop_table[shop_nr].close == 12)
      strcat(buf, "noon");
    else if (shop_table[shop_nr].close == 24)
      strcat(buf, "midnight");
    else
      sprintf(ENDOF(buf), "%dpm", shop_table[shop_nr].close - 12);
  }
  strcat(buf, ".\r\n");
  send_to_char(buf, ch);
}

SPECIAL(shop_keeper)
{
  struct char_data *keeper = (struct char_data *) me;
  vnum_t shop_nr;
  if (!cmd)
    return FALSE;
  for (shop_nr = 0; shop_nr < top_of_shopt; shop_nr++)
    if (shop_table[shop_nr].keeper == GET_MOB_VNUM(keeper))
      break;
  if (shop_nr >= top_of_shopt)
    return FALSE;

  skip_spaces(&argument);
  if (CMD_IS("buy"))
    shop_buy(argument, ch, keeper, shop_nr);
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
  else if (CMD_IS("receive"))
    shop_rec(argument, ch, keeper, shop_nr);
  else if (CMD_IS("hours"))
    shop_hours(ch, shop_nr);
  else if (CMD_IS("cancel"))
    shop_cancel(argument, ch, keeper, shop_nr);
  else
    return FALSE;
  return TRUE;
}

void assign_shopkeepers(void)
{
  int index, rnum;
  cmd_say = find_command("say");
  for (index = 0; index < top_of_shopt; index++) {
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
  for (int i = 0; i < top_of_shopt; i++) {
    if (shop_table[i].random_amount)
      shop_table[i].random_current = number(-shop_table[i].random_amount, shop_table[i].random_amount);
    for (struct shop_sell_data *sell = shop_table[i].selling; sell; sell = sell->next)
      for (int q = 0; q <= 4; q++)
        sell->lastidnum[q] = 0;
  }
}

void list_detailed_shop(struct char_data *ch, vnum_t shop_nr)
{
  sprintf(buf, "Vnum:       [%5ld], Rnum: [%5ld]\r\n", shop_table[shop_nr].vnum, shop_nr);
  sprintf(ENDOF(buf), "Name: %30s Shopkeeper: %s [%5ld]\r\n", shop_table[shop_nr].shopname,
                       mob_proto[real_mobile(shop_table[shop_nr].keeper)].player.physical_text.name,
          shop_table[shop_nr].keeper);
  sprintf(ENDOF(buf), "Buy at:     [%1.2f], Sell at: [%1.2f], \xC2\xB1 %%: [%d], Current %%: [%d], Hours [%d-%d]\r\n",
          shop_table[shop_nr].profit_buy, shop_table[shop_nr].profit_sell, shop_table[shop_nr].random_amount,
          shop_table[shop_nr].random_current, shop_table[shop_nr].open, shop_table[shop_nr].close);
  sprintf(ENDOF(buf), "Type:       %s, Ettiquette: %s\r\n", shop_type[shop_table[shop_nr].type], skills[shop_table[shop_nr].ettiquete].name);
  shop_table[shop_nr].races.PrintBits(buf2, MAX_STRING_LENGTH, pc_race_types, NUM_RACES);
  sprintf(ENDOF(buf), "!Serves:     %s\r\n", buf2);
  shop_table[shop_nr].flags.PrintBits(buf2, MAX_STRING_LENGTH, shop_flags, SHOP_FLAGS);
  sprintf(ENDOF(buf), "Flags:      %s\r\n", buf2);
  shop_table[shop_nr].buytypes.PrintBits(buf2, MAX_STRING_LENGTH, item_types, NUM_ITEMS);
  sprintf(ENDOF(buf), "Buytypes:   %s\r\n", buf2);
  strcat(buf, "Selling: \r\n");
  for (struct shop_sell_data *selling = shop_table[shop_nr].selling; selling; selling = selling->next)
    sprintf(ENDOF(buf), "%-50s (%5ld) Type: %s Amount: %d\r\n", obj_proto[real_object(selling->vnum)].text.name,
            selling->vnum, selling_type[selling->type], selling->stock);
  page_string(ch->desc, buf, 0);
}

void write_shops_to_disk(int zone)
{
  long counter, realcounter;
  FILE *fp;
  struct shop_data *shop;
  zone = real_zone(zone);
  sprintf(buf, "%s/%d.shp", SHP_PREFIX, zone_table[zone].number);
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
              "Open:\t%d\n"
              "Close:\t%d\n"
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
  send_to_char(CH, "q) Return to Main Menu\r\nEnter Your Choice: ");
  d->edit_mode = SHEDIT_TEXT_MENU;
}

void shedit_disp_selling_menu(struct descriptor_data *d)
{
  CLS(CH);
  int i = 1;
  for (struct shop_sell_data *sell = SHOP->selling; sell; sell = sell->next, i++)
  {

    sprintf(buf, "%d) ^c%-50s^n (^c%5ld^n) Type: ^c%6s^n", i, GET_OBJ_NAME(&obj_proto[real_object(sell->vnum)]),
            sell->vnum, selling_type[sell->type]);
    if (sell->type == SELL_STOCK)
      sprintf(ENDOF(buf), " Stock: ^c%d^n", sell->stock);
    strcat(buf, "\r\n");
    send_to_char(buf, CH);
  }
  send_to_char("a) Add Entry\r\nd) Delete Entry\r\nEnter Choice (0 to quit)", CH);
  d->edit_mode = SHEDIT_SELLING_MENU;
}

void shedit_disp_menu(struct descriptor_data *d)
{
  CLS(CH);
  send_to_char(CH, "Shop Number: %ld\r\n", SHOP->vnum);
  send_to_char(CH, "1) Keeper: ^c%ld^n (^c%s^n)\r\n", SHOP->keeper,
               real_mobile(SHOP->keeper) > 0 ? GET_NAME(&mob_proto[real_mobile(SHOP->keeper)]) : "NULL");
  send_to_char(CH, "2) Type: ^c%s^n\r\n", shop_type[SHOP->type]);
  send_to_char(CH, "3) Buying Profit: ^c%.2f^n\r\n", SHOP->profit_buy);
  send_to_char(CH, "4) Selling Profit: ^c%.2f^n\r\n", SHOP->profit_sell);
  send_to_char(CH, "5) %% \xC2\xB1: ^c%d^n\r\n", SHOP->random_amount);
  send_to_char(CH, "6) Opens: ^c%d^n Closes: ^c%d^n\r\n", SHOP->open, SHOP->close);
  send_to_char(CH, "7) Etiquette: ^c%s^n\r\n", skills[SHOP->ettiquete].name);
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
      if (!from_ip_zone(d->edit_number)) {
        sprintf(buf,"%s wrote new shop #%ld",
                GET_CHAR_NAME(d->character), d->edit_number);
        mudlog(buf, d->character, LOG_WIZLOG, TRUE);
      }
      shop_nr = real_shop(d->edit_number);
      if (shop_nr > 0) {
        rnum_t okn, nkn;
        if (shop_table[shop_nr].keeper != SHOP->keeper && shop_table[shop_nr].keeper != 1151) {
          okn = real_mobile(shop_table[shop_nr].keeper);
          nkn = real_mobile(SHOP->keeper);
          if (mob_index[okn].func == shop_keeper) {
            mob_index[okn].func = mob_index[okn].sfunc;
            mob_index[okn].sfunc = NULL;
          } else if (mob_index[okn].sfunc == shop_keeper)
            mob_index[okn].sfunc = NULL;
          mob_index[nkn].sfunc = mob_index[nkn].func;
          mob_index[nkn].func = shop_keeper;
        }
        SHOP->order = shop_table[shop_nr].order;
        SHOP->vnum = d->edit_number;
        free_shop(shop_table + shop_nr);
        shop_table[shop_nr] = *SHOP;
      } else {
        vnum_t counter, counter2;
        bool found = FALSE;
        for (counter = 0; counter < top_of_shopt + 1; counter++) {
          if (!found)
            if (shop_table[counter].vnum > d->edit_number) {
              for (counter2 = top_of_shopt + 1; counter2 > counter; counter2--)
                shop_table[counter2] = shop_table[counter2 - 1];
              SHOP->vnum = d->edit_number;
              shop_table[counter] = *(d->edit_shop);
              found = TRUE;
            }
        }
        if (!found) {
          SHOP->vnum = d->edit_number;
          shop_table[top_of_shopt + 1] = *SHOP;
        }
        top_of_shopt++;
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
      send_to_char(CH, "0) Grey\r\n1) Legal\r\n2) Black\r\nEnter Shop Type: ");
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
                   "Enter Etiquette skill required for availibility tests: ");
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
    if (profit < 0) {
      send_to_char("Profit must be greater than 0! Enter profit for sell command: ", CH);
      return;
    }
    SHOP->profit_buy = profit;
    shedit_disp_menu(d);
    break;
  case SHEDIT_PROFIT_SELL:
    profit = atof(arg);
    if (profit < 0) {
      send_to_char("Profit must be greater than 0! Enter profit for sell command: ", CH);
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
      send_to_char("0) Always\r\n1) Availibility Code\r\n2) Limited Stock\r\nEnter Supply Type: ", CH);
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

