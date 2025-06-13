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



  todo: add table, test

  stretch: convert immediate DB calls to calls to memory
*/

#include <mysql/mysql.h>

#include "interpreter.hpp"
#include "utils.hpp"
#include "handler.hpp"
#include "db.hpp"
#include "newdb.hpp"

// Determines if object is stowable, TRUE or FALSE. Sends an error to user on FALSE.
bool obj_can_be_stowed(struct char_data *ch, struct obj_data *obj, bool send_messages) {
  FALSE_CASE_PRINTF_MO(GET_OBJ_CONDITION(obj) / MAX(1, GET_OBJ_BARRIER(obj)) < 1, "%s is too damaged to be stowed.", GET_OBJ_NAME(obj));
  FALSE_CASE_PRINTF_MO(obj->contains, "%s has contents, so it can't be stowed.", GET_OBJ_NAME(obj));
  FALSE_CASE_PRINTF_MO(GET_OBJ_QUEST_CHAR_ID(obj), "%s is a quest object and can't be stowed.", GET_OBJ_NAME(obj));
  FALSE_CASE_PRINTF_MO(GET_OBJ_RNUM(obj) <= 0 || GET_OBJ_RNUM(obj) > top_of_objt, "%s is a temporary object that can't be stowed.", GET_OBJ_NAME(obj));
  FALSE_CASE_PRINTF_MO(obj->restring || obj->photo || obj->graffiti, "Customized objects like %s can't be stowed.", GET_OBJ_NAME(obj));
  FALSE_CASE_PRINTF_MO(get_soulbound_idnum(obj) > 0, "You can't stow soulbound items like %s.", GET_OBJ_NAME(obj));

  struct obj_data *proto = &obj_proto[GET_OBJ_RNUM(obj)];

  FALSE_CASE_PRINTF_MO(GET_OBJ_COST(obj) != GET_OBJ_COST(proto), "%s's cost has changed from the prototype value, so it can't be stowed.", GET_OBJ_NAME(obj));

  FALSE_CASE_PRINTF_MO(obj->obj_flags.extra_flags == proto->obj_flags.extra_flags,
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
  snprintf(query_buf, sizeof(query_buf), "INSERT INTO pfiles_stowed (idnum, vnum, qty) VALUES (%ld, %ld, 1) ON DUPLICATE KEY UPDATE qty=qty+1;", GET_IDNUM(ch), GET_OBJ_VNUM(obj));
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
  snprintf(query_buf, sizeof(query_buf), "UPDATE pfiles_stowed SET qty=qty-1 WHERE idnum = %ld AND vnum = %ld;", GET_IDNUM(ch), GET_OBJ_VNUM(&obj_proto[rnum]));

  // Execute query and confirm it worked.
  if (mysql_wrapper(mysql, query_buf)) {
    mudlog_vfprintf(ch, LOG_SYSLOG, "SYSERR: Failed to decrement qty of stowed item w/ vnum %ld. Bailing.", GET_OBJ_VNUM(&obj_proto[rnum]));
    send_to_char(ch, "That didn't work. Please contact staff.\r\n");
    return;
  }

  // Read it out and give it to them
  struct obj_data *obj = read_object(rnum, REAL, OBJ_LOAD_REASON_UNSTOW_CMD, 0, GET_IDNUM(ch));
  obj_to_char(obj, ch);
  send_to_char(ch, "You pull %s out of stowage.\r\n", GET_OBJ_NAME(obj));
  return;
}

#define STOW_SYNTAX "Syntax: STOW <item>; UNSTOW <item>; STOWED. You can sell your stowage with SELL STOWED at a vendor."
ACMD(do_stow) {
  MYSQL_RES *res;
  MYSQL_ROW row;
  char query_buf[MAX_INPUT_LENGTH + 100] = {0};

  FAILURE_CASE(IS_NPC(ch), "Sorry, you can't use the STOW command in your current state.");

  // 'stowed' lists contents of storage
  if (subcmd == SCMD_LIST_STOWED) {
    // todo: list storage
    snprintf(query_buf, sizeof(query_buf), "SELECT vnum, qty FROM pfiles_stowed WHERE idnum = %ld AND qty >= 1 ORDER BY qty DESC;", GET_IDNUM(ch));
    
    if (mysql_wrapper(mysql, query_buf)) {
      send_to_char(ch, "Sorry, your storage space isn't working right now.\r\n");
      mudlog_vfprintf(ch, LOG_SYSLOG, "SYSERR: failed to query storage for %s in list mode", GET_CHAR_NAME(ch));
      return;
    }
    
    if (!(res = mysql_use_result(mysql))) {
      send_to_char(ch, "Sorry, your storage space isn't working right now.\r\n");
      mudlog_vfprintf(ch, LOG_SYSLOG, "SYSERR: failed to use_result when querying storage for %s in list mode", GET_CHAR_NAME(ch));
      return;
    }
  
    int total_value = 0;
    send_to_char(ch, "Your storage space contains:\r\n");
    while ((row = mysql_fetch_row(res))) {
      vnum_t vnum = atol(row[0]);
      rnum_t rnum = real_object(vnum);
      long qty = atol(row[1]);
      if (rnum <= 0) {
        mudlog_vfprintf(ch, LOG_SYSLOG, "SYSERR: Object with unrecognized vnum %ld found in storage space.", vnum);
        continue;
      }
      struct obj_data *proto = &obj_proto[rnum];
      send_to_char(ch, "- %50s (x%ld)", GET_OBJ_NAME(proto), qty);
      total_value += IS_OBJ_STAT(proto, ITEM_EXTRA_NOSELL) ? 0 : GET_OBJ_COST(proto);
    }
    send_to_char(ch, "Total value is somewhere between %.2f and %.2f nuyen.", total_value * 0.05, total_value * 0.25);
    mysql_free_result(res);
    return;
  }

  skip_spaces(&argument);

  // 'stow' and 'unstown' on their own gives syntax
  FAILURE_CASE(!*argument, STOW_SYNTAX);

  // 'stow X' puts the thing away
  if (subcmd == SCMD_STOW) {
    struct obj_data *obj = get_obj_in_list_vis(ch, argument, ch->carrying);
    FAILURE_CASE_PRINTF(!obj, "You're not carrying anything named '%s'.", argument);
    if (!obj_can_be_stowed(ch, obj, TRUE))
      return;
    // Stow it. Obj should be assumed extracted after this call, so don't reference it.
    raw_stow_obj(ch, obj, TRUE);
    return;
  }

  // 'unstow X' gets a new copy of the thing out
  if (subcmd == SCMD_UNSTOW) {
    // Make sure they have enough inventory space.
    FAILURE_CASE(IS_CARRYING_N(ch) + 1 > CAN_CARRY_N(ch), "You can't carry any more items.");

    // Search through stowed things for something matching the keyword.    
    snprintf(query_buf, sizeof(query_buf), "SELECT vnum FROM pfiles_stowed WHERE idnum = %ld AND qty >= 1;", GET_IDNUM(ch));
    
    if (mysql_wrapper(mysql, query_buf)) {
      send_to_char(ch, "Sorry, your storage space isn't working right now.\r\n");
      mudlog_vfprintf(ch, LOG_SYSLOG, "SYSERR: failed to query storage for %s", GET_CHAR_NAME(ch));
      return;
    }
    
    if (!(res = mysql_use_result(mysql))) {
      send_to_char(ch, "Sorry, your storage space isn't working right now.\r\n");
      mudlog_vfprintf(ch, LOG_SYSLOG, "SYSERR: failed to use_result when querying storage for %s", GET_CHAR_NAME(ch));
      return;
    }
  
    while ((row = mysql_fetch_row(res))) {
      vnum_t vnum = atol(row[0]);
      rnum_t rnum = real_object(vnum);
      if (rnum <= 0) {
        mudlog_vfprintf(ch, LOG_SYSLOG, "SYSERR: Object with unrecognized vnum %ld found in storage space.", vnum);
        continue;
      }
      if (keyword_appears_in_obj(argument, &obj_proto[rnum])) {
        mysql_free_result(res);
        unstow_obj(ch, rnum);
        return;
      }
    }

    send_to_char(ch, "You don't have anything matching '%s' in your stowage space.\r\n", argument);
    mysql_free_result(res);
    return;
  }
}