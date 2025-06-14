/* For storing loot items for sale.
  - Must match prototype in all forms (undamaged, empty, etc)
  - stow such items with STOW/STORE command
  - sell with SELL STOWED / SELL STORED
  - unstow with UNSTOW/UNSTORE command
  - list with STOWED/STORED


  table:
  idnum = pc idnum
  vnum = obj vnum
  qty = number of these things stowed

  pk: idnum + vnum together



  todo: test

  stretch: convert immediate DB calls to calls to memory
*/

#include <mysql/mysql.h>
#include <unordered_map>
#include <vector>

#include "interpreter.hpp"
#include "utils.hpp"
#include "handler.hpp"
#include "db.hpp"
#include "newdb.hpp"

#define STOWED_MAP_T std::unordered_map<vnum_t, long>

extern int negotiate_and_payout_sellprice(struct char_data *ch, struct char_data *keeper, vnum_t shop_nr, int sellprice);
extern int sell_price(struct obj_data *obj, vnum_t shop_nr, idnum_t faction_idnum, struct char_data *ch);
extern bool shop_will_buy_item_from_ch(rnum_t shop_nr, struct obj_data *obj, struct char_data *ch);
extern struct shop_data *shop_table;

STOWED_MAP_T *get_stowed_item_list(idnum_t idnum, STOWED_MAP_T *map_to_populate) {
  map_to_populate->clear();

  char query_buf[1000];
  snprintf(query_buf, sizeof(query_buf), "SELECT vnum, qty FROM pfiles_stowed WHERE idnum = %ld AND qty >= 1 ORDER BY qty DESC;", idnum);
    
  if (mysql_wrapper(mysql, query_buf)) {
    mudlog_vfprintf(NULL, LOG_SYSLOG, "SYSERR: failed to query stowage for %ld in list mode", idnum);
    return NULL;
  }

  MYSQL_RES *res;
  MYSQL_ROW row;
  
  if (!(res = mysql_use_result(mysql))) {
    mudlog_vfprintf(NULL, LOG_SYSLOG, "SYSERR: failed to use_result when querying stowage for %s in list mode", idnum);
    return NULL;
  }

  while ((row = mysql_fetch_row(res))) {
    vnum_t vnum = atol(row[0]);

    // Validate it's a real object.
    if (real_object(vnum) <= 0) {
      mudlog_vfprintf(NULL, LOG_SYSLOG, "SYSERR: Object with unrecognized vnum %ld found in stowage space for %ld.", vnum, idnum);
      continue;
    }

    map_to_populate->emplace(vnum, atol(row[1]));
  }

  mysql_free_result(res);
  return map_to_populate;
}

// Determines if object is stowable, TRUE or FALSE. Sends an error to user on FALSE.
bool obj_can_be_stowed(struct char_data *ch, struct obj_data *obj, bool send_messages) {
  FALSE_CASE_PRINTF_MO(GET_OBJ_CONDITION(obj) / MAX(1, GET_OBJ_BARRIER(obj)) < 1, "%s is too damaged to be stowed.", GET_OBJ_NAME(obj));
  FALSE_CASE_PRINTF_MO(obj->contains, "%s has contents, so it can't be stowed.", GET_OBJ_NAME(obj));
  FALSE_CASE_PRINTF_MO(GET_OBJ_QUEST_CHAR_ID(obj), "%s is a quest object and can't be stowed.", GET_OBJ_NAME(obj));
  FALSE_CASE_PRINTF_MO(GET_OBJ_RNUM(obj) <= 0 || GET_OBJ_RNUM(obj) > top_of_objt, "%s is a temporary object that can't be stowed.", GET_OBJ_NAME(obj));
  FALSE_CASE_PRINTF_MO(obj->restring || obj->photo || obj->graffiti, "Customized objects like %s can't be stowed.", GET_OBJ_NAME(obj));
  FALSE_CASE_PRINTF_MO(get_soulbound_idnum(obj) > 0, "You can't stow soulbound items like %s.", GET_OBJ_NAME(obj));
  FALSE_CASE_PRINTF_MO(GET_OBJ_TYPE(obj) == ITEM_SHOPCONTAINER, "You can't stow shop containers.");
  FALSE_CASE_PRINTF_MO(GET_OBJ_TYPE(obj) == ITEM_VEHCONTAINER, "You can't stow vehicles.");
  FALSE_CASE_PRINTF_MO(GET_OBJ_TYPE(obj) == ITEM_MONEY, "You can't stow credsticks or nuyen.");

  struct obj_data *proto = &obj_proto[GET_OBJ_RNUM(obj)];

  FALSE_CASE_PRINTF_MO(GET_OBJ_COST(obj) != GET_OBJ_COST(proto), "%s's cost has changed from the prototype value, so it can't be stowed.", GET_OBJ_NAME(obj));

  FALSE_CASE_PRINTF_MO(strcmp(obj->obj_flags.extra_flags.ToString(), proto->obj_flags.extra_flags.ToString()),
                         "%s's flags don't match the prototype, so it can't be stowed.", GET_OBJ_NAME(obj));

  for (int val_idx = 0; val_idx < NUM_OBJ_VALUES; val_idx++) {
    if (GET_OBJ_VAL(obj, val_idx) != GET_OBJ_VAL(proto, val_idx)) {
      // We don't care about certain things like weapon firemode.
      if (GET_OBJ_TYPE(obj) == ITEM_WEAPON && WEAPON_IS_GUN(obj) && val_idx == 11)
        continue;

      if (send_messages) {
        send_to_char(ch, "You can only stow objects that are unaltered from their default state, and %s's value %d has been changed.\r\n", GET_OBJ_NAME(obj), val_idx);
      }
      return FALSE;
    }
  }

  return TRUE;
}

// Skip all checks and stow the object. You should NEVER call this without checking obj_can_be_stowed() first.
bool raw_stow_obj(struct char_data *ch, struct obj_data *obj, bool send_messages) {
  if (!ch || !obj) {
    mudlog_vfprintf(ch, LOG_SYSLOG, "invalid raw_store_obj(%s, %s, %s)", GET_CHAR_NAME(ch), GET_OBJ_NAME(obj), send_messages ? "T" : "F");
    return FALSE;
  }

  // Put it in their hammer space.
  char query_buf[1000] = {0};
  snprintf(query_buf, sizeof(query_buf), "INSERT INTO pfiles_stowed (idnum, vnum, qty) VALUES (%ld, %ld, 1) ON DUPLICATE KEY UPDATE qty=qty+1;", GET_IDNUM_EVEN_IF_PROJECTING(ch), GET_OBJ_VNUM(obj));
  if (mysql_wrapper(mysql, query_buf)) {
    mudlog_vfprintf(ch, LOG_SYSLOG, "SYSERR: Unable to stow item due to mysql_wrapper failure.");
    if (send_messages) {
      send_to_char(ch, "Uh-oh, stowing %s didn't work. Please contact staff. Your object has been returned to you.\r\n", GET_OBJ_NAME(obj));
    }
    return FALSE;
  } else {
    extract_obj(obj);
    return TRUE;
  }
}

void unstow_obj(struct char_data *ch, rnum_t rnum) {
  char query_buf[1000];

  // Prepare to decrease stored qty by 1.
  snprintf(query_buf, sizeof(query_buf), "UPDATE pfiles_stowed SET qty=qty-1 WHERE idnum = %ld AND vnum = %ld;", GET_IDNUM_EVEN_IF_PROJECTING(ch), GET_OBJ_VNUM(&obj_proto[rnum]));

  // Execute query and confirm it worked.
  if (mysql_wrapper(mysql, query_buf)) {
    mudlog_vfprintf(ch, LOG_SYSLOG, "SYSERR: Failed to decrement qty of stowed item w/ vnum %ld. Bailing.", GET_OBJ_VNUM(&obj_proto[rnum]));
    send_to_char(ch, "That didn't work. Please contact staff.\r\n");
    return;
  }

  // Read it out and give it to them
  struct obj_data *obj = read_object(rnum, REAL, OBJ_LOAD_REASON_UNSTOW_CMD, 0, GET_IDNUM_EVEN_IF_PROJECTING(ch));
  obj_to_char(obj, ch);
  send_to_char(ch, "You pull %s out of stowage.\r\n", GET_OBJ_NAME(obj));
  return;
}

void list_stowage_to_character(struct char_data *ch, idnum_t victim) {
  STOWED_MAP_T stowed_map = {};
  int total_value = 0;
  bool viewer_is_victim = (GET_IDNUM_EVEN_IF_PROJECTING(ch) == victim);

  FAILURE_CASE_PRINTF(!get_stowed_item_list(GET_IDNUM_EVEN_IF_PROJECTING(ch), &stowed_map), "Sorry, %s stowage space isn't working right now.", viewer_is_victim ? "your" : "their'");
  FAILURE_CASE_PRINTF(stowed_map.empty(), "%s stowage space is empty.", viewer_is_victim ? "Your" : "Their'");

  send_to_char(ch, "%s stowage space contains:\r\n", viewer_is_victim ? "your" : "their'");
  for (auto iter : stowed_map) {
    struct obj_data *proto = &obj_proto[real_object(iter.first)];
    if (iter.second > 1) {
      send_to_char(ch, "  %s^n  (^c%ld^n)\r\n", GET_OBJ_NAME(proto), iter.second);
    } else {
      send_to_char(ch, "  %s^n\r\n", GET_OBJ_NAME(proto));
    }
    total_value += (IS_OBJ_STAT(proto, ITEM_EXTRA_NOSELL) ? 0 : GET_OBJ_COST(proto)) * iter.second;
  }
  if (IS_SENATOR(ch)) {
    send_to_char(ch, "Total value is %d, which upon selling works out to somewhere between %d and %d nuyen.\r\n", total_value, (int) (total_value * 0.05), (int) (total_value * 0.25));
  } else {
    send_to_char(ch, "Total value is somewhere between %d and %d nuyen.\r\n", (int) (total_value * 0.05), (int) (total_value * 0.25));
  }
}

#define STOW_SYNTAX "Syntax: STOW <item>; UNSTOW <item>; STOWED. You can sell your stowage with SELL STOWED at a vendor."
ACMD(do_stow) {
  FAILURE_CASE(IS_NPC(ch), "Sorry, you can't use the STOW command in your current state.");

  // 'stowed' lists contents of stowage
  if (subcmd == SCMD_LIST_STOWED) {
    list_stowage_to_character(ch, GET_IDNUM_EVEN_IF_PROJECTING(ch));
    return;
  }

  skip_spaces(&argument);

  // 'stow X' puts the thing away
  if (subcmd == SCMD_STOW) {
    // 'stow' on its own gives syntax
    FAILURE_CASE(!*argument, STOW_SYNTAX);

    struct obj_data *obj = get_obj_in_list_vis(ch, argument, ch->carrying);
    FAILURE_CASE_PRINTF(!obj, "You're not carrying anything named '%s'.", argument);
    if (!obj_can_be_stowed(ch, obj, TRUE))
      return;
    // Stow it. Obj should be assumed extracted after this call, so don't reference it.
    send_to_char(ch, "You stow %s away for fencing.\r\n", decapitalize_a_an(obj));
    raw_stow_obj(ch, obj, TRUE);
    return;
  }

  // 'unstow X' gets a new copy of the thing out
  if (subcmd == SCMD_UNSTOW) {
    // 'unstow' on its own gives list of items
    if (!*argument) {
      list_stowage_to_character(ch, GET_IDNUM_EVEN_IF_PROJECTING(ch));
      send_to_char(ch, "\r\nSyntax to get an item back out: UNSTOW <keyword>\r\n");
      return;
    }

    STOWED_MAP_T stowed_map = {};

    // Make sure they have enough inventory space.
    FAILURE_CASE(IS_CARRYING_N(ch) + 1 > CAN_CARRY_N(ch), "You can't carry any more items.");
    FAILURE_CASE(!get_stowed_item_list(GET_IDNUM_EVEN_IF_PROJECTING(ch), &stowed_map), "Sorry, your stowage space isn't working right now.");
    FAILURE_CASE(stowed_map.empty(), "Your stowage space is empty.");
  
    for (auto iter : stowed_map) {
      rnum_t rnum = real_object(iter.first);
      if (keyword_appears_in_obj(argument, &obj_proto[rnum])) {
        unstow_obj(ch, rnum);
        return;
      }
    }

    send_to_char(ch, "You don't have anything matching '%s' in your stowage space.\r\n", argument);
    return;
  }
}

void sell_all_stowed_items(struct char_data *ch, rnum_t shop_nr, struct char_data *keeper) {
  // check for invalid ch/keeper
  if (!ch || !keeper || shop_nr < 0 || shop_nr > top_of_shopt) {
    mudlog_vfprintf(ch, LOG_SYSLOG, "SYSERR: Invalid invocation of sell_all_stowed_items(%s, %ld, %s)", GET_CHAR_NAME(ch), shop_nr, GET_CHAR_NAME(keeper));
    return;
  }

  STOWED_MAP_T stowed_map = {};
  std::vector<vnum_t> to_drop_from_table = {};

  FAILURE_CASE(!get_stowed_item_list(GET_IDNUM_EVEN_IF_PROJECTING(ch), &stowed_map), "Sorry, your stowage space isn't working right now.");
  FAILURE_CASE(stowed_map.empty(), "Your stowage space is empty.");

  int total_sell_price = 0;
  int total_sold_items = 0;
  char query_buf[1000];

  // for each item in list, if it's sellable here, add its value to the total (multiply by qty) and queue it for dropping from table
  for (auto iter : stowed_map) {
    struct obj_data *proto = &obj_proto[real_object(iter.first)];

    if (!shop_will_buy_item_from_ch(shop_nr, proto, ch))
      continue;

    total_sell_price += sell_price(proto, shop_nr, GET_MOB_FACTION_IDNUM(keeper), ch);
    to_drop_from_table.push_back(GET_OBJ_VNUM(proto));
    total_sold_items += iter.second;
  }

  if (total_sold_items <= 0) {
    act("$N isn't interested in buying any of your stowed items.", FALSE, ch, 0, keeper, TO_CHAR);
    return;
  }

  // Drop items from table. On error, bail.
  snprintf(query_buf, sizeof(query_buf), "DELETE FROM pfiles_stowed WHERE idnum=%ld AND vnum IN (", GET_IDNUM_EVEN_IF_PROJECTING(ch));
  bool first_run = TRUE;
  for (auto vnum : to_drop_from_table) {
    snprintf(ENDOF(query_buf), sizeof(query_buf) - strlen(query_buf), "%s%ld", first_run ? "" : ", ", vnum);
    first_run = FALSE;
  }

  if (mysql_wrapper(mysql, query_buf)) {
    send_to_char(ch, "Something went wrong. Your items have been returned to you.\r\n");
    mudlog_vfprintf(ch, LOG_SYSLOG, "SYSERR: Failed to execute query to delete stowed items during sell.");
    return;
  }

  negotiate_and_payout_sellprice(ch, keeper, shop_nr, total_sell_price);
  
  send_to_char(ch, "You sell %d item%s for %d nuyen.\r\n", total_sold_items, total_sold_items == 1 ? "" : "s", total_sell_price);
}