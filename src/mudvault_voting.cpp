/* file: mudvault_voting.cpp
 * contents: MudVault (mudvault.org) voting + character-linking integration.
 *
 * ARCHITECTURE: The game NEVER talks to the network. Players vote at
 * mudvault.org; a Python stack (scripts/, do not touch from here) performs all
 * HTTP communication with MudVault and exchanges data with the game solely via
 * the shared MySQL database. This module heartbeats the DB, delivers rewards
 * to online players, and processes character-verification results.
 *
 * COMPILE GATE: the integration (boot/heartbeat/verify plumbing) is behind
 * -DMUDVAULT_VOTING (see mudvault_voting.hpp for the no-op stubs used when it
 * is off); do_vote/do_verify at the bottom always compile, with their own
 * "not enabled" branches. The pfile columns mudvault_verified/last_vote_time
 * are required in EVERY build (load_char()/save_char() are not gated), and
 * boot_world() enforces their presence/tail order unconditionally.
 *
 * Schema: SQL/Migrations/add_votes.sql. Secrets (API keys) live ONLY in the
 * gitignored src/mysql_config.cpp -- never copy key values elsewhere or into
 * source control.
 */

/* Includes are deliberately NOT gated on MUDVAULT_VOTING: do_vote/do_verify
 * at the bottom of this file compile in BOTH configurations, so this TU needs
 * its headers either way (the codebase already requires MySQL headers/libs in
 * every build). Only the integration itself is behind the gate. */
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include <mysql/mysql.h>

#include "structs.hpp"
#include "awake.hpp"
#include "db.hpp"         // for descriptor_list
#include "comm.hpp"
#include "interpreter.hpp" // for ACMD, one_argument
#include "utils.hpp"
#include "newdb.hpp"      // for mysql, mysql_wrapper, prepare_quotes, SaveChar
#include "mysql_config.hpp"

#include "mudvault_voting.hpp"

#ifdef MUDVAULT_VOTING

/* --- file-static state --------------------------------------------------- */

static bool mv_boot_ran = FALSE;
static bool mv_enabled = FALSE;
static int mv_mud_id_value = -1;
static char mv_vote_url_buf[64];

#define MV_LINK_BATCH_SIZE 20
#define MV_VOTE_BATCH_SIZE 20

/* Copy of a mudvault_character_linking row we are about to act on, so we can
 * free the MYSQL_RES before issuing follow-up queries on the connection. */
struct mv_linking_row {
  long idnum;
  char character_name[256];
  bool succeeded;
};

/* Copy of an undelivered mudvault_votes row. */
struct mv_vote_row {
  long idnum;
  char character_name[64];
  char reward_id_hex[40];
  long voted_at_epoch;
};

/* --- helpers -------------------------------------------------------------- */

static struct char_data *find_online_playing_char_by_idnum(idnum_t idnum)
{
  if (idnum <= 0)
    return NULL;

  for (struct descriptor_data *d = descriptor_list; d; d = d->next) {
    if (STATE(d) != CON_PLAYING || !d->character)
      continue;
    if (GET_IDNUM(d->character) == idnum)
      return d->character;
  }
  return NULL;
}

/* reward_id_hex is a DB-generated uuid string, but sanity-check it before we
 * splice it into a query anyway (hex digits + dashes only, 36 chars). */
static bool reward_id_hex_is_kosher(const char *hex)
{
  if (!hex || strlen(hex) != 36)
    return FALSE;

  for (const char *p = hex; *p; p++) {
    if (*p != '-' && !isxdigit((unsigned char) *p))
      return FALSE;
  }
  return TRUE;
}

/* Atomically claim (set redeemed_at) an undelivered vote row and, if the
 * claim succeeds, grant the reward to ch. The affected-rows check makes this
 * idempotent under double-processing (heartbeat vs login hook races). */
static void deliver_vote_reward(struct char_data *ch, const char *reward_id_hex, long voted_at_epoch)
{
  char query[512];

  if (!ch || !reward_id_hex || !reward_id_hex_is_kosher(reward_id_hex))
    return;

  /* Defense-in-depth: the DB (MySQL trigger on mudvault_votes) is the primary
   * enforcement of the verified-gate; this re-check mirrors do_vote's gating
   * so an unverified char can never be handed a reward even if a bad row
   * somehow lands in the table. */
  if (!GET_MUDVAULT_VERIFIED(ch))
    return;

  snprintf(query, sizeof(query),
           "UPDATE mudvault_votes SET redeemed_at = NOW() "
           "WHERE reward_id_bin = UNHEX(REPLACE('%s', '-', '')) AND redeemed_at IS NULL",
           reward_id_hex);

  if (mysql_wrapper(mysql, query))
    return;

  if (mysql_affected_rows(mysql) != 1) {
    /* Someone else (a concurrent heartbeat / login pass) already claimed it. */
    return;
  }

  /* Claimed -- grant the reward. (ch comes from the online descriptor list,
   * so it is a PC with a real player_specials; write the pfile fields
   * directly -- the GET_* macros above are read-only.) */
  ch->player_specials->saved.last_vote_time = voted_at_epoch;
  gain_syspoints(ch, 1, false, "mudvault vote reward");

  mudlog_vfprintf(ch, LOG_SYSLOG, "MUDVAULT: %s redeemed vote reward %s.",
                  GET_CHAR_NAME(ch), reward_id_hex);

  send_to_char(ch, "Your vote on MudVault was received - 1 system point awarded. Vote again in 24 hours: %s\r\n",
               mv_vote_url());
}

/* Process one linking-result row, per the heartbeat/on-login contract. */
static void process_linking_result(long idnum, const char *character_name, bool succeeded)
{
  struct char_data *ch = find_online_playing_char_by_idnum(idnum);

  if (!ch) {
    /* Offline: leave the row for their login hook to process. */
    return;
  }

  if (succeeded) {
    ch->player_specials->saved.mudvault_verified = TRUE;
    SaveChar(ch);

    send_to_char(ch, "Your MudVault profile is now linked to %s. Use VOTE to collect your vote rewards!\r\n",
                 character_name ? character_name : "(unknown)");
    mudlog_vfprintf(ch, LOG_SYSLOG, "MUDVAULT: Link succeeded for %s (%ld).",
                    GET_CHAR_NAME(ch), idnum);

    /* Ack so Python stops re-reporting it. Guard on the status so we can't
     * clobber a freshly re-submitted 'needs_linking' row in a race. */
    char query[256];
    snprintf(query, sizeof(query),
             "UPDATE mudvault_character_linking SET linking_status = 'confirmed' "
             "WHERE idnum = %ld AND linking_status = 'linking_succeeded'",
             idnum);
    mysql_wrapper(mysql, query);
  } else {
    send_to_char(ch, "Your MudVault verification code was invalid or expired - get a fresh code at mudvault.org/profile and run VERIFY <code> again.\r\n");
    mudlog_vfprintf(ch, LOG_SYSLOG, "MUDVAULT: Link failed for %s (%ld).",
                    GET_CHAR_NAME(ch), idnum);
    /* Consume the row: it has served its purpose (one-shot result report).
     * Leaving it would make the heartbeat re-message the player every 30
     * seconds until they re-verify. Re-running VERIFY inserts/overwrites a
     * fresh row via ON DUPLICATE KEY, so nothing is lost. Guarded on the
     * status so we can't delete a freshly re-submitted 'needs_linking' row. */
    char query[256];
    snprintf(query, sizeof(query),
             "DELETE FROM mudvault_character_linking "
             "WHERE idnum = %ld AND linking_status = 'linking_failed'",
             idnum);
    mysql_wrapper(mysql, query);
  }
}

/* --- public API ----------------------------------------------------------- */

bool mv_boot()
{
  if (mv_boot_ran)
    return mv_enabled;
  mv_boot_ran = TRUE;

  /* Parse "mv_<mud_id>_<secret>". */
  const char *key = mudvault_api_key;
  if (!key || strncmp(key, "mv_", 3) != 0) {
    mudlog_vfprintf(NULL, LOG_SYSLOG, "MudVault: malformed API key, voting integration disabled");
    return mv_enabled = FALSE;
  }

  const char *digits = key + 3;
  char id_buf[16];
  size_t n = 0;
  while (isdigit(*digits)) {
    if (n < sizeof(id_buf) - 1)
      id_buf[n++] = *digits;
    digits++;
  }
  id_buf[n] = '\0';

  if (n == 0 || *digits != '_') {
    mudlog_vfprintf(NULL, LOG_SYSLOG, "MudVault: malformed API key, voting integration disabled");
    return mv_enabled = FALSE;
  }

  int mud_id = atoi(id_buf);
  if (mud_id <= 0) {
    mudlog_vfprintf(NULL, LOG_SYSLOG, "MudVault: malformed API key, voting integration disabled");
    return mv_enabled = FALSE;
  }

  /* The mudvault tables store DATETIMEs as SERVER-LOCAL wall time (whatever
   * the DB's default time zone is; Python writes datetime.now()-style local
   * values and the game reads them back via UNIX_TIMESTAMP(), which
   * interprets them in the session zone). Verify that this connection's
   * NOW() agrees with the game host's local wall clock, or the 24h vote
   * cooldown would be silently skewed -- refuse to run otherwise. This also
   * catches a DB moved to a host in a different time zone or with drifting
   * clock; the fix is to align the DB server's time zone/clock with the game
   * host's. */
  {
    bool tz_ok = FALSE;
    long skew = 0;

    if (!mysql_wrapper(mysql, "SELECT NOW();")) {
      MYSQL_RES *res = mysql_store_result(mysql);
      MYSQL_ROW row = res ? mysql_fetch_row(res) : NULL;
      if (row && row[0]) {
        struct tm db_wall;
        memset(&db_wall, 0, sizeof(db_wall));
        if (strptime(row[0], "%Y-%m-%d %H:%M:%S", &db_wall)) {
          db_wall.tm_isdst = -1; /* strptime doesn't set it; let mktime resolve DST */
          time_t db_epoch = mktime(&db_wall);
          if (db_epoch != (time_t) -1) {
            skew = (long) db_epoch - (long) time(NULL);
            tz_ok = (skew > -120 && skew < 120);
          }
        }
      }
      if (res)
        mysql_free_result(res);
    }

    if (!tz_ok) {
      mudlog_vfprintf(NULL, LOG_SYSLOG,
                      "MudVault: MySQL server's wall clock disagrees with the game host's local time "
                      "(skew %ld seconds) -- voting integration disabled. Align the DB server's time "
                      "zone and clock with the game host's.",
                      skew);
      return mv_enabled = FALSE;
    }
  }

  mv_mud_id_value = mud_id;
  snprintf(mv_vote_url_buf, sizeof(mv_vote_url_buf), "https://mudvault.org/?id=%d", mud_id);
  mv_enabled = TRUE;

  mudlog_vfprintf(NULL, LOG_SYSLOG, "MudVault: voting integration enabled (mud id %d).", mud_id);
  return TRUE;
}

int mv_mud_id()
{
  if (!mv_boot_ran || !mv_enabled)
    return -1;
  return mv_mud_id_value;
}

const char *mv_vote_url()
{
  if (!mv_boot_ran || !mv_enabled)
    return NULL;
  return mv_vote_url_buf;
}

void mv_heartbeat()
{
  if (!mv_enabled)
    return;

  /* Build the list of idnums that are online RIGHT NOW. Both the linking and
   * vote scans are restricted to these idnums: an offline player's rows are
   * handled by mv_on_login when they next connect. Restricting the linking
   * scan matters for liveness -- without it, LIMIT 20 can be permanently
   * occupied by stale linking results for chars who never log in again
   * (process_linking_result leaves those rows for the login hook, so they
   * would starve every other player's linking result forever). */
  char idlist[2048] = "";
  size_t idlist_len = 0;
  int online_count = 0;

  for (struct descriptor_data *d = descriptor_list; d; d = d->next) {
    if (STATE(d) != CON_PLAYING || !d->character || GET_IDNUM(d->character) <= 0)
      continue;
    int printed = snprintf(idlist + idlist_len, sizeof(idlist) - idlist_len,
                           "%s%ld", online_count ? "," : "", GET_IDNUM(d->character));
    if (printed < 0 || (size_t)printed >= sizeof(idlist) - idlist_len)
      break; /* list full -- any remaining online players are covered next heartbeat */
    idlist_len += printed;
    online_count++;
  }

  char query[2560];

  if (online_count > 0) {
    /* 1. Process character-verification (linking) results. */
    {
      MYSQL_RES *res = NULL;
      MYSQL_ROW row;
      struct mv_linking_row rows[MV_LINK_BATCH_SIZE];
      int num_rows = 0;

      snprintf(query, sizeof(query),
               "SELECT idnum, character_name, linking_status FROM mudvault_character_linking "
               "WHERE linking_status IN ('linking_succeeded', 'linking_failed') "
               "AND idnum IN (%s) LIMIT 20",
               idlist);

      if (!mysql_wrapper(mysql, query)) {
        if ((res = mysql_store_result(mysql))) {
          while (num_rows < MV_LINK_BATCH_SIZE && (row = mysql_fetch_row(res))) {
            rows[num_rows].idnum = row[0] ? atol(row[0]) : 0;
            strlcpy(rows[num_rows].character_name, row[1] ? row[1] : "", sizeof(rows[num_rows].character_name));
            rows[num_rows].succeeded = (row[2] && !strcmp(row[2], "linking_succeeded"));
            num_rows++;
          }
          mysql_free_result(res);
        }
      }

      /* Result is freed -- safe to issue follow-up queries now. */
      for (int i = 0; i < num_rows; i++)
        process_linking_result(rows[i].idnum, rows[i].character_name, rows[i].succeeded);
    }

    /* 2. Deliver undelivered vote rewards to online players. The scan is
     * restricted to idnums that are online RIGHT NOW: this guarantees an online
     * player can never be starved behind MV_VOTE_BATCH_SIZE stale rows that
     * belong to characters who never log in (those are delivered by mv_on_login
     * whenever their owner finally does). */
    {
      MYSQL_RES *res = NULL;
      MYSQL_ROW row;
      struct mv_vote_row rows[MV_VOTE_BATCH_SIZE];
      int num_rows = 0;

      snprintf(query, sizeof(query),
               "SELECT idnum, character_name, reward_id_hex, UNIX_TIMESTAMP(voted_at) FROM mudvault_votes "
               "WHERE redeemed_at IS NULL AND idnum IN (%s) LIMIT 20",
               idlist);

      if (!mysql_wrapper(mysql, query)) {
        if ((res = mysql_store_result(mysql))) {
          while (num_rows < MV_VOTE_BATCH_SIZE && (row = mysql_fetch_row(res))) {
            if (!row[0] || !row[1] || !row[2] || !row[3])
              continue;
            rows[num_rows].idnum = atol(row[0]);
            strlcpy(rows[num_rows].character_name, row[1], sizeof(rows[num_rows].character_name));
            strlcpy(rows[num_rows].reward_id_hex, row[2], sizeof(rows[num_rows].reward_id_hex));
            rows[num_rows].voted_at_epoch = atol(row[3]);
            num_rows++;
          }
          mysql_free_result(res);
        }
      }

      for (int i = 0; i < num_rows; i++) {
        struct char_data *ch = find_online_playing_char_by_idnum(rows[i].idnum);
        if (!ch) {
          /* Logged out between the query and now: row stays; mv_on_login
           * handles it next time they connect. */
          continue;
        }
        deliver_vote_reward(ch, rows[i].reward_id_hex, rows[i].voted_at_epoch);
      }
    }
  }

  /* 3. Best-effort cleanup of stale acknowledged unlink requests. */
  mysql_wrapper(mysql,
                "DELETE FROM mudvault_character_linking "
                "WHERE linking_status = 'unlinking_succeeded' AND created < (NOW() - INTERVAL 7 DAY)");
}

void mv_on_login(struct char_data *ch)
{
  if (!mv_enabled || !ch || GET_IDNUM(ch) <= 0)
    return;

  long idnum = GET_IDNUM(ch);

  /* Deliver any undelivered vote rewards for this char (by idnum). */
  {
    MYSQL_RES *res = NULL;
    MYSQL_ROW row;
    struct mv_vote_row rows[MV_VOTE_BATCH_SIZE];
    int num_rows = 0;

    char query[256];
    snprintf(query, sizeof(query),
             "SELECT character_name, reward_id_hex, UNIX_TIMESTAMP(voted_at) FROM mudvault_votes "
             "WHERE idnum = %ld AND redeemed_at IS NULL LIMIT 20",
             idnum);

    if (!mysql_wrapper(mysql, query)) {
      if ((res = mysql_store_result(mysql))) {
        while (num_rows < MV_VOTE_BATCH_SIZE && (row = mysql_fetch_row(res))) {
          if (!row[0] || !row[1] || !row[2])
            continue;
          strlcpy(rows[num_rows].character_name, row[0], sizeof(rows[num_rows].character_name));
          strlcpy(rows[num_rows].reward_id_hex, row[1], sizeof(rows[num_rows].reward_id_hex));
          rows[num_rows].voted_at_epoch = atol(row[2]);
          num_rows++;
        }
        mysql_free_result(res);
      }
    }

    for (int i = 0; i < num_rows; i++)
      deliver_vote_reward(ch, rows[i].reward_id_hex, rows[i].voted_at_epoch);
  }

  /* Check linking results for this idnum. */
  {
    MYSQL_RES *res = NULL;
    MYSQL_ROW row;
    char character_name[256] = "";
    bool succeeded = FALSE;
    bool have_result = FALSE;

    char query[256];
    snprintf(query, sizeof(query),
             "SELECT character_name, linking_status FROM mudvault_character_linking WHERE idnum = %ld LIMIT 1",
             idnum);

    if (!mysql_wrapper(mysql, query)) {
      if ((res = mysql_store_result(mysql))) {
        if ((row = mysql_fetch_row(res))) {
          strlcpy(character_name, row[0] ? row[0] : "", sizeof(character_name));
          if (row[1] && !strcmp(row[1], "linking_succeeded")) {
            succeeded = TRUE;
            have_result = TRUE;
          } else if (row[1] && !strcmp(row[1], "linking_failed")) {
            succeeded = FALSE;
            have_result = TRUE;
          }
        }
        mysql_free_result(res);
      }
    }

    if (have_result)
      process_linking_result(idnum, character_name, succeeded);
  }
}

int mv_submit_verification(struct char_data *ch, const char *code)
{
  if (!mv_enabled || !ch || !code)
    return MV_VERIFY_DB_ERROR;

  /* Validate the code: 4..10 alphanumeric characters (case-insensitive per API). */
  size_t len = strlen(code);
  if (len < 4 || len > 10)
    return MV_VERIFY_BAD_FORMAT;

  for (const char *p = code; *p; p++) {
    if (!isalnum((unsigned char) *p))
      return MV_VERIFY_BAD_FORMAT;
  }

  if (GET_IDNUM(ch) <= 0 || !GET_CHAR_NAME(ch))
    return MV_VERIFY_DB_ERROR;

  char prepared_name[512];
  char prepared_code[64];

  if (!prepare_quotes(prepared_name, GET_CHAR_NAME(ch), sizeof(prepared_name)) ||
      !prepare_quotes(prepared_code, code, sizeof(prepared_code)))
    return MV_VERIFY_DB_ERROR;

  char query[1024];
  snprintf(query, sizeof(query),
           "INSERT INTO mudvault_character_linking (idnum, character_name, verification_code, linking_status) "
           "VALUES (%ld, '%s', '%s', 'needs_linking') "
           "ON DUPLICATE KEY UPDATE character_name = '%s', verification_code = '%s', linking_status = 'needs_linking'",
           GET_IDNUM(ch), prepared_name, prepared_code, prepared_name, prepared_code);

  if (mysql_wrapper(mysql, query)) {
    mudlog_vfprintf(ch, LOG_SYSLOG, "SYSERR: MUDVAULT: Failed to submit verification code for %s (%ld).",
                    GET_CHAR_NAME(ch), GET_IDNUM(ch));
    return MV_VERIFY_DB_ERROR;
  }

  mudlog_vfprintf(ch, LOG_SYSLOG, "MUDVAULT: %s (%ld) submitted a verification code for linking.",
                  GET_CHAR_NAME(ch), GET_IDNUM(ch));
  return MV_VERIFY_OK;
}

/* Shared query builder for mv_request_unlink_by_id(). */
static void mv_write_unlink_row(long idnum, const char *character_name)
{
  /* verification_code is NOT NULL VARCHAR(36) with no default, and the char's
   * old code may be gone at this point, so we store the placeholder string
   * "unlink-requested" (Python treats a needs_unlinking row by its status, not
   * its code). */
  char prepared_name[512];
  if (!prepare_quotes(prepared_name, character_name, sizeof(prepared_name)))
    return;

  char query[1024];
  snprintf(query, sizeof(query),
           "INSERT INTO mudvault_character_linking (idnum, character_name, verification_code, linking_status) "
           "VALUES (%ld, '%s', 'unlink-requested', 'needs_unlinking') "
           "ON DUPLICATE KEY UPDATE character_name = '%s', verification_code = 'unlink-requested', "
           "linking_status = 'needs_unlinking'",
           idnum, prepared_name, prepared_name);

  if (mysql_wrapper(mysql, query)) {
    mudlog_vfprintf(NULL, LOG_SYSLOG, "SYSERR: MUDVAULT: Failed to write unlink request for %s (%ld).",
                    prepared_name, idnum);
    return;
  }

  mudlog_vfprintf(NULL, LOG_SYSLOG, "MUDVAULT: Wrote unlink request for %s (%ld).", prepared_name, idnum);

  /* Orphan cleanup: any pending (unredeemed) vote rewards for this idnum are
   * dead rows -- idnums are never reused, so no future character can redeem
   * them. Drop them so MudVault-side pending state and our table stay clean. */
  char delete_query[256];
  snprintf(delete_query, sizeof(delete_query),
           "DELETE FROM mudvault_votes WHERE idnum = %ld AND redeemed_at IS NULL",
           idnum);
  mysql_wrapper(mysql, delete_query);
}

void mv_request_unlink_by_id(long idnum, const char *character_name)
{
  if (!mv_enabled || idnum <= 0 || !character_name || !*character_name)
    return;

  mv_write_unlink_row(idnum, character_name);
}

#endif /* MUDVAULT_VOTING */

/* ---- Player-facing commands ----
 * These live outside the module #ifdef so they exist even when
 * -DMUDVAULT_VOTING is off (cmd_info references them unconditionally); each
 * one carries its own #ifndef branch with a friendly "not enabled" message. */

ACMD(do_vote)
{
#ifndef MUDVAULT_VOTING
  send_to_char("Voting rewards are not enabled on this server.\r\n", ch);
  return;
#else
  const char *url = mv_vote_url();

  if (!url) {
    send_to_char("MudVault voting is not configured.\r\n", ch);
    return;
  }

  if (!GET_MUDVAULT_VERIFIED(ch)) {
    send_to_char(ch, "You haven't linked your MudVault profile yet.\r\n"
                     "1. Visit ^Whttps://mudvault.org/profile^n and log in.\r\n"
                     "2. Click ^WRewards^n, then link a new character for ^WAwakeMUD CE^n to get a verification code.\r\n"
                     "3. Type: ^Wverify <code>^n\r\n"
                     "4. Then vote for us at ^W%s^n (or just ^WVOTE^n once linked).\r\n", url);
    return;
  }

  if (GET_LAST_VOTE_TIME(ch) == 0) {
    send_to_char(ch, "Thanks for linking your profile! You haven't voted yet -- vote at ^W%s^n and your reward will arrive automatically.\r\n", url);
    return;
  }

  long since = time(NULL) - GET_LAST_VOTE_TIME(ch);

  if (since < MUDVAULT_VOTE_COOLDOWN_SECS) {
    long remaining = MUDVAULT_VOTE_COOLDOWN_SECS - since;
    int hours = remaining / 3600;
    int minutes = (remaining % 3600) / 60;

    char remaining_str[64];
    if (hours && minutes)
      snprintf(remaining_str, sizeof(remaining_str), "%d hour%s %d minute%s", hours, hours == 1 ? "" : "s", minutes, minutes == 1 ? "" : "s");
    else if (hours)
      snprintf(remaining_str, sizeof(remaining_str), "%d hour%s", hours, hours == 1 ? "" : "s");
    else if (minutes)
      snprintf(remaining_str, sizeof(remaining_str), "%d minute%s", minutes, minutes == 1 ? "" : "s");
    else
      strlcpy(remaining_str, "less than a minute", sizeof(remaining_str));

    time_t last_vote = (time_t) GET_LAST_VOTE_TIME(ch);
    char *tmstr = asctime(localtime(&last_vote));
    *(tmstr + strlen(tmstr) - 1) = '\0';

    send_to_char(ch, "You last voted at %s. You can vote again in %s.\r\n", tmstr, remaining_str);
    return;
  }

  send_to_char(ch, "You can vote again! Visit ^W%s^n -- your 1 system point reward arrives automatically after voting.\r\n", url);
#endif /* MUDVAULT_VOTING */
}

ACMD(do_verify)
{
#ifndef MUDVAULT_VOTING
  send_to_char("Voting rewards are not enabled on this server.\r\n", ch);
  return;
#else
  const char *url = mv_vote_url();

  if (!url) {
    send_to_char("MudVault voting is not configured.\r\n", ch);
    return;
  }

  char code[MAX_INPUT_LENGTH];
  one_argument(argument, code);

  if (!*code) {
    send_to_char(ch, "Usage: ^Wverify <code>^n\r\n"
                     "1. Visit ^Whttps://mudvault.org/profile^n and log in.\r\n"
                     "2. Click ^WRewards^n, then link a new character for ^WAwakeMUD CE^n to get a verification code.\r\n"
                     "3. Type: ^Wverify <code>^n\r\n"
                     "4. Then vote for us at ^W%s^n (or just ^WVOTE^n once linked).\r\n", url);
    return;
  }

  int status = mv_submit_verification(ch, code);

  if (status == MV_VERIFY_OK) {
    send_to_char(ch, "Verification submitted for %s - you will be notified when it is processed (usually within a few minutes).\r\n"
                     "If the code is invalid you will be told; grab a fresh one at ^Whttps://mudvault.org/profile^n.\r\n",
                 GET_CHAR_NAME(ch));
  } else if (status == MV_VERIFY_BAD_FORMAT) {
    send_to_char("That doesn't look like a valid verification code (4-10 letters/numbers). Get a fresh code at ^Whttps://mudvault.org/profile^n and try: ^Wverify <code>^n\r\n", ch);
  } else {
    send_to_char(ch, "Something went wrong submitting your verification - please try again later or contact staff.\r\n"
                     "The failure has been logged.\r\n");
  }
#endif /* MUDVAULT_VOTING */
}
