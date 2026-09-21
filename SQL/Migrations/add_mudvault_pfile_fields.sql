-- Adds per-player MudVault voting state to pfiles.
--
-- IMPORTANT: load_char() (src/newdb.cpp) reads pfiles columns POSITIONALLY via
-- "SELECT * FROM pfiles", so these columns MUST be appended at the end of the
-- table. Do NOT use an AFTER clause or re-order columns. Appending at the end
-- yields row[90] = mudvault_verified and row[91] = last_vote_time, matching
-- the indices used in load_char() (the highest existing index is row[89],
-- RestrictedSysPoints). Inserting these anywhere else will corrupt character
-- loading.

ALTER TABLE `pfiles`
  ADD COLUMN `mudvault_verified` TINYINT(1) NOT NULL DEFAULT 0,
  ADD COLUMN `last_vote_time` BIGINT NOT NULL DEFAULT 0;
