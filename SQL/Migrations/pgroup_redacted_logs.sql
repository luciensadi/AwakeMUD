ALTER TABLE pgroup_logs ADD redacted BOOL NOT NULL;
UPDATE pgroup_logs SET redacted = FALSE;