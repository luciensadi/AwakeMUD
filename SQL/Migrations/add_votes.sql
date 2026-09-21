-- MudVault voting integration (mudvault.org).
--
-- Who writes what:
--   * The Python web/API stack inserts rows into `mudvault_character_linking`
--     (link requests) and `mudvault_votes` (one row per vote, with the reward
--     UUID once the reward is granted).
--   * The C++ game reads `mudvault_votes` and sets `redeemed_at` when the
--     reward is actually delivered to the player in-game.
--   * Python sets `claimed_at` when the MudVault API claim succeeds -- only
--     ever after `redeemed_at` has been set by the game.
--
-- Intended MySQL scoped users:
--   * Web user: INSERT-only on `mudvault_votes` (+ SELECT on pfiles name/idnum
--     and on `mudvault_character_linking` for the linking trigger lookups).
--   * Cron user: read/write on all `mudvault_*` tables.

-- Per-player voting state, appended to pfiles. The explicit AFTER chain is
-- load-bearing: load_char() (src/newdb.cpp) reads pfiles POSITIONALLY via
-- "SELECT * FROM pfiles", and expects these as the FINAL two columns:
--   row[90] = mudvault_verified, row[91] = last_vote_time
-- (the highest pre-existing index is row[89], RestrictedSysPoints). Chaining
-- each column to the previous one is what guarantees that; inserting them
-- anywhere else corrupts character loading. db.cpp's
-- require_that_fields_end_table() re-checks the full tail order at boot.
-- Note: the ALTER is not re-runnable (MySQL has no ADD COLUMN IF NOT EXISTS);
-- re-running after it has been applied fails with "Duplicate column name".
ALTER TABLE `pfiles`
  ADD COLUMN `mudvault_verified` TINYINT(1) NOT NULL DEFAULT 0 AFTER `RestrictedSysPoints`,
  ADD COLUMN `last_vote_time` BIGINT NOT NULL DEFAULT 0 AFTER `mudvault_verified`;

-- Guards so the rest of this migration can be re-run.
DROP TRIGGER IF EXISTS mudvault_votes_before_insert;
DROP TABLE IF EXISTS `mudvault_votes`;
DROP TABLE IF EXISTS `mudvault_character_linking`;

CREATE TABLE `mudvault_character_linking` (
  `idnum` MEDIUMINT(6) UNSIGNED NOT NULL UNIQUE,
  `character_name` VARCHAR(256) NOT NULL,
  `created` DATETIME DEFAULT CURRENT_TIMESTAMP,
  `verification_code` VARCHAR(36) NOT NULL,
  `linking_status` ENUM('needs_linking', 'linking_succeeded', 'linking_failed', 'needs_unlinking', 'unlinking_succeeded', 'confirmed'),
  PRIMARY KEY (`idnum`)
);

-- MySQL (unlike MariaDB) rejects stored functions in generated column
-- expressions, so the binary(16) -> dashed lowercase UUID conversion is
-- inlined as builtin expressions in each *_hex column below.

CREATE TABLE `mudvault_votes` (
  `idnum` MEDIUMINT(6) UNSIGNED NOT NULL,

  `character_name` VARCHAR(256) NOT NULL,

  `vote_id_bin` binary(16) NULL DEFAULT NULL UNIQUE,
  `vote_id_hex` varchar(36) GENERATED ALWAYS AS (LOWER(INSERT(INSERT(INSERT(INSERT(HEX(`vote_id_bin`),9,0,'-'),14,0,'-'),19,0,'-'),24,0,'-'))) VIRTUAL,

  `reward_id_bin` binary(16) NOT NULL,
  `reward_id_hex` varchar(36) GENERATED ALWAYS AS (LOWER(INSERT(INSERT(INSERT(INSERT(HEX(`reward_id_bin`),9,0,'-'),14,0,'-'),19,0,'-'),24,0,'-'))) VIRTUAL,

  `reward_token_bin` binary(16) NULL DEFAULT NULL,
  `reward_token_hex` varchar(36) GENERATED ALWAYS AS (LOWER(INSERT(INSERT(INSERT(INSERT(HEX(`reward_token_bin`),9,0,'-'),14,0,'-'),19,0,'-'),24,0,'-'))) VIRTUAL,

  `voted_at` DATETIME NOT NULL,
  `redeemed_at` DATETIME NULL DEFAULT NULL,
  `claimed_at` DATETIME NULL DEFAULT NULL,

  PRIMARY KEY (`reward_id_bin`)
);

-- `character_name` is required: the Python stack INSERTs it, the trigger
-- below resolves it to a verified idnum, and the C++ game SELECTs it.

-- Resolve the character's idnum from their name at insert time.
--
-- The name is only honored if it belongs to a MudVault-VERIFIED character
-- (a 'confirmed' row in mudvault_character_linking). This closes a hijack
-- race: if a verified character is deleted (unlink queued, pending rewards
-- still in flight at MudVault) and their name is reused by a new, unverified
-- character, the trigger rejects the insert instead of awarding the old
-- player's reward to the new owner. The reward stays pending at MudVault and
-- will succeed once the name's new owner verifies -- or can be claimed
-- manually.
DELIMITER //
CREATE TRIGGER mudvault_votes_before_insert
BEFORE INSERT ON `mudvault_votes`
FOR EACH ROW
BEGIN
  DECLARE linked_idnum MEDIUMINT(6) UNSIGNED;

  SELECT p.idnum INTO linked_idnum
  FROM pfiles p
  JOIN mudvault_character_linking l ON l.idnum = p.idnum
  WHERE p.name = NEW.character_name
    AND l.character_name = NEW.character_name
    AND l.linking_status = 'confirmed'
  LIMIT 1;

  IF linked_idnum IS NULL THEN
    SIGNAL SQLSTATE '45000' SET MESSAGE_TEXT = 'mudvault_votes: character_name is not a MudVault-verified character';
  END IF;

  SET NEW.idnum = linked_idnum;
END //
DELIMITER ;
