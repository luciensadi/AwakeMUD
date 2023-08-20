/* ************************************************************************
*   File: ban.c                                         Part of CircleMUD *
*  Usage: banning/unbanning/checking sites and player names               *
*                                                                         *
*  All rights reserved.  See license.doc for complete information.        *
*                                                                         *
*  Copyright (C) 1993, 94 by the Trustees of the Johns Hopkins University *
*  CircleMUD is based on DikuMUD, Copyright (C) 1990, 1991.               *
************************************************************************ */

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <time.h>
#include <sys/types.h>

#include "awake.hpp"
#include "structs.hpp"
#include "utils.hpp"
#include "comm.hpp"
#include "interpreter.hpp"
#include "handler.hpp"
#include "db.hpp"

struct ban_list_element *ban_list = NULL;

const char *ban_types[] = {
                      "no",
                      "new",
                      "select",
                      "all",
                      "ERROR"
                    };

void load_banned(void)
{
  FILE *fl;
  int i, date;
  char site_name[BANNED_SITE_LENGTH + 1], ban_type[100];
  char name[MAX_NAME_LENGTH + 1];
  struct ban_list_element *next_node;

  ban_list = 0;

  if (!(fl = fopen(BAN_FILE, "r"))) {
    perror("Unable to open banfile");
    return;
  }
  while (fscanf(fl, " %99s %50s %d %20s ", ban_type, site_name, &date, name) == 4) {
    next_node = new ban_list_element;
    strlcpy(next_node->site, site_name, BANNED_SITE_LENGTH);
    next_node->site[BANNED_SITE_LENGTH] = '\0';
    strlcpy(next_node->name, name, MAX_NAME_LENGTH);
    next_node->name[MAX_NAME_LENGTH] = '\0';
    next_node->date = date;

    for (i = BAN_NOT; i <= BAN_ALL; i++)
      if (!strcmp(ban_type, ban_types[i]))
        next_node->type = i;

    next_node->next = ban_list;
    ban_list = next_node;
  }

  fclose(fl);
}

int isbanned(char *hostname)
{
  if (!hostname || !*hostname)
    return BAN_NOT;

  int i = BAN_NOT;
  for (struct ban_list_element *banned_node = ban_list; banned_node; banned_node = banned_node->next) {
    if (str_str((const char *)hostname, banned_node->site))      /* if hostname is a substring */
      i = MAX(i, banned_node->type);
  }

  return i;
}

void _write_one_node(FILE * fp, struct ban_list_element * node)
{
  if (node)
  {
    _write_one_node(fp, node->next);
    fprintf(fp, "%s %s %ld %s\n", ban_types[node->type],
            node->site, (long) node->date, node->name);
  }
}

void write_ban_list(void)
{
  FILE *fl;

  if (!(fl = fopen(BAN_FILE, "w"))) {
    perror("write_ban_list");
    return;
  }
  _write_one_node(fl, ban_list);/* recursively write from end to start */
  fclose(fl);
  return;
}

void _create_ban_entry(const char *site, int ban_type, const char *banned_by) {
  struct ban_list_element *ban_node = new ban_list_element;

  // Copy in the site name.
  strlcpy(ban_node->site, site, BANNED_SITE_LENGTH);

  // Lower-case it.
  for (char *nextchar = ban_node->site; *nextchar; nextchar++)
    *nextchar = LOWER(*nextchar);

  // Write out who banned it.
  strlcpy(ban_node->name, banned_by, MAX_NAME_LENGTH);

  // And when.
  ban_node->date = time(0);

  // And what type it is.
  ban_node->type = ban_type;

  // Add it to our active ban list.
  ban_node->next = ban_list;
  ban_list = ban_node;

  // Save bans to disk.
  write_ban_list();

  // Log it for posterity.
  mudlog_vfprintf(NULL, LOG_BANLOG, "%s has banned %s for %s players.", 
                  CAP(banned_by),
                  site,
                  ban_types[ban_type]);
}

// Adds or changes a ban node for the current site. Returns the type of node it was.
int add_or_rewrite_ban_entry(const char *site, int ban_type, const char *banned_by) {
  struct ban_list_element *ban_node;

  for (ban_node = ban_list; ban_node; ban_node = ban_node->next) {
    if (!str_cmp(ban_node->site, site)) {
      int old_node_type = ban_node->type;
      // Ban exists, so rewrite it.
      mudlog_vfprintf(NULL, LOG_BANLOG, "Ban for %s (%s by %s) overwritten to %s by %s.", 
                      ban_node->site, 
                      ban_types[ban_node->type], 
                      ban_node->name, 
                      ban_type, 
                      banned_by);
      ban_node->type = ban_type;
      write_ban_list();
      return old_node_type;
    }
  }

  // Ban didn't exist, so write a new one.
  _create_ban_entry(site, ban_type, banned_by);
  return BAN_NOT;
}

ACMD(do_ban)
{
  char flag[MAX_INPUT_LENGTH], site[MAX_INPUT_LENGTH];
  char *timestr;
  int i;
  struct ban_list_element *ban_node;

  *buf = '\0';

  if (!*argument) {
    if (!ban_list) {
      send_to_char("No sites are banned.\r\n", ch);
      return;
    }
#define FORMAT_STR "%-25.25s  %-8.8s  %-10.10s  %-16.16s\r\n"
    snprintf(buf, sizeof(buf), FORMAT_STR, // Flawfinder: ignore
            "Banned Site Name",
            "Ban Type",
            "Banned On",
            "Banned By");
    send_to_char(buf, ch);
    snprintf(buf, sizeof(buf), FORMAT_STR, // Flawfinder: ignore
            "---------------------------------",
            "---------------------------------",
            "---------------------------------",
            "---------------------------------");
    send_to_char(buf, ch);

    for (ban_node = ban_list; ban_node; ban_node = ban_node->next) {
      if (ban_node->date) {
        timestr = asctime(localtime(&(ban_node->date)));
        *(timestr + 10) = 0;
        strlcpy(site, timestr, sizeof(site));
      } else
        strlcpy(site, "Unknown", sizeof(site));
      snprintf(buf, sizeof(buf), FORMAT_STR, // Flawfinder: ignore
               ban_node->site, ban_types[ban_node->type], site, ban_node->name);
      send_to_char(buf, ch);
    }
    return;
#undef FORMAT_STR
  }
  two_arguments(argument, flag, site);
  
  FAILURE_CASE(!*site || !*flag, "Usage: ban {all | select | new} site_name");
  FAILURE_CASE(!(!str_cmp(flag, "select") || !str_cmp(flag, "all") || !str_cmp(flag, "new")), "Flag must be ALL, SELECT, or NEW.");

  int ban_type = -1;
  for (i = BAN_NEW; i <= BAN_ALL; i++) {
    if (!str_cmp(flag, ban_types[i])) {
      ban_type = i;
      break;
    }
  }

  FAILURE_CASE(ban_type == -1, "Invalid ban type specified.");

  add_or_rewrite_ban_entry(site, ban_type, GET_CHAR_NAME(ch));

  send_to_char("Site banned.\r\n", ch);
}

ACMD(do_unban)
{
  char site[MAX_INPUT_LENGTH];
  struct ban_list_element *ban_node, *temp;
  int found = 0;

  one_argument(argument, site);
  if (!*site) {
    send_to_char("A site to unban might help.\r\n", ch);
    return;
  }
  ban_node = ban_list;
  while (ban_node && !found) {
    if (!str_cmp(ban_node->site, site))
      found = 1;
    else
      ban_node = ban_node->next;
  }

  if (!found) {
    send_to_char("That site is not currently banned.\r\n", ch);
    return;
  }
  REMOVE_FROM_LIST(ban_node, ban_list, next);
  send_to_char("Site unbanned.\r\n", ch);
  snprintf(buf, sizeof(buf), "%s removed the %s-player ban on %s.",
          GET_CHAR_NAME(ch), ban_types[ban_node->type], ban_node->site);
  mudlog(buf, ch, LOG_WIZLOG, TRUE);

  DELETE_AND_NULL(ban_node);
  write_ban_list();
}

extern bool _GLOBALLY_BAN_OPENVPN_CONNETIONS_;

ACMD(do_banvpn) {
  _GLOBALLY_BAN_OPENVPN_CONNETIONS_ = !_GLOBALLY_BAN_OPENVPN_CONNETIONS_;
  mudlog_vfprintf(ch, LOG_SYSLOG, "Game-wide OpenVPN origin port ban set to %s by %s.", _GLOBALLY_BAN_OPENVPN_CONNETIONS_ ? "ON" : "OFF", GET_CHAR_NAME(ch));
}
