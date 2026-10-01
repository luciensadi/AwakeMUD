/* mudvault_voting.hpp
 *
 * MudVault (mudvault.org) voting + character-linking integration.
 *
 * ARCHITECTURE: Players vote on the MudVault website. A Python stack (see
 * scripts/, owned by the web side) owns ALL network communication with
 * mudvault.org and talks to the game only through the shared MySQL database.
 * The C++ game NEVER makes HTTP calls. Schema lives in
 * SQL/Migrations/add_votes.sql; API keys live ONLY in the gitignored
 * src/mysql_config.cpp. See that file's header for the contract details.
 */

#ifndef _mudvault_voting_h_
#define _mudvault_voting_h_

#define MUDVAULT_VOTE_COOLDOWN_SECS (24 * 60 * 60)

/* Status codes for mv_submit_verification(). */
#define MV_VERIFY_OK          0 /* queued for processing */
#define MV_VERIFY_BAD_FORMAT  1 /* invalid input (caller prints usage) */
#define MV_VERIFY_DB_ERROR    2 /* DB/internal failure (already logged) */

/* The entire integration is gated behind -DMUDVAULT_VOTING. When the flag is
 * off, the declarations below become inline no-ops: all hooks compile away,
 * the heartbeat/boot do nothing, and the user-facing commands (do_vote /
 * do_verify in act.other.cpp) print a friendly "not enabled" message instead. */
#ifdef MUDVAULT_VOTING

/* Validate the API key (gitignored mysql_config.cpp) and derive our mud id.
 * Must be called once at boot before anything else works; safe to call again
 * (idempotent). Returns FALSE if the subsystem is disabled (malformed key). */
bool mv_boot();

/* Mud id, parsed from the "mv_<id>_..." prefix of mudvault_api_key.
 * Returns -1 if the key is malformed or boot has not run. */
int mv_mud_id();

/* "https://mudvault.org/?id=<id>" in a static buffer, or NULL if the mud id
 * is invalid. */
const char *mv_vote_url();

/* Called every ~30 RL seconds from comm.cpp. No-op if boot failed. Scans the
 * mudvault_* tables and delivers rewards/linking results. */
void mv_heartbeat();

/* Deliver pending vote rewards + linking results for ch after they enter the
 * game. Safe on chars without descriptors / mid-login. */
void mv_on_login(struct char_data *ch);

/* Write a 'needs_linking' row with the player-supplied code. Returns
 * MV_VERIFY_OK, MV_VERIFY_BAD_FORMAT (bad input; caller prints usage), or
 * MV_VERIFY_DB_ERROR (internal failure, already logged; caller prints a
 * try-again-later message). */
int mv_submit_verification(struct char_data *ch, const char *code);

/* Write a 'needs_unlinking' row (character deletion). Called from DeleteChar,
 * so it only needs the idnum/name (no loaded char). */
void mv_request_unlink_by_id(long idnum, const char *character_name);

#else /* !MUDVAULT_VOTING: compiled-out no-op implementations */

inline bool mv_boot() { return FALSE; }
inline int mv_mud_id() { return -1; }
inline const char *mv_vote_url() { return NULL; }
inline void mv_heartbeat() {}
inline void mv_on_login(struct char_data *ch) { (void) ch; }
inline int mv_submit_verification(struct char_data *ch, const char *code) { (void) ch; (void) code; return MV_VERIFY_DB_ERROR; }
inline void mv_request_unlink_by_id(long idnum, const char *character_name) { (void) idnum; (void) character_name; }

#endif // MUDVAULT_VOTING

#endif // ifndef _mudvault_voting_h_
